#import "platform/apple/HaylenVideoLoader.h"

#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>

#include <algorithm>
#include <cstdint>
#include <exception>
#include <span>
#include <utility>

@implementation HaylenVideoLoader {
    std::shared_ptr<haylen::io::PackageReader> file;
    NSString* contentType;
}

- (instancetype)initWithFile:(std::shared_ptr<haylen::io::PackageReader>)reader extension:(NSString*)extension {
    self = [super init];
    file = std::move(reader);
    UTType* type = [UTType typeWithFilenameExtension:extension];
    contentType = type != nil ? type.identifier : UTTypeMPEG4Movie.identifier;
    return self;
}

// Ranges reach AVFoundation in pieces of a megabyte, so a long request never holds the whole file in memory.
- (BOOL)resourceLoader:(AVAssetResourceLoader*)resourceLoader shouldWaitForLoadingOfRequestedResource:(AVAssetResourceLoadingRequest*)request {
    constexpr std::uint64_t kPieceSize = 1U << 20U;
    const std::uint64_t size = file->getSize();
    if (AVAssetResourceLoadingContentInformationRequest* information = request.contentInformationRequest) {
        information.contentType = contentType;
        information.contentLength = static_cast<long long>(size);
        information.byteRangeAccessSupported = YES;
    }
    if (AVAssetResourceLoadingDataRequest* data = request.dataRequest) {
        const auto offset = static_cast<std::uint64_t>(std::max<long long>(data.requestedOffset, 0));
        const std::uint64_t end = data.requestsAllDataToEndOfResource ? size : std::min(size, offset + static_cast<std::uint64_t>(data.requestedLength));
        try {
            for (std::uint64_t at = offset; at < end && !request.isCancelled; at += kPieceSize) {
                NSMutableData* piece = [NSMutableData dataWithLength:static_cast<NSUInteger>(std::min(kPieceSize, end - at))];
                const std::size_t read = file->read(at, std::span<std::uint8_t>(static_cast<std::uint8_t*>(piece.mutableBytes), piece.length));
                piece.length = read;
                [data respondWithData:piece];
                if (read == 0) {
                    break;
                }
            }
        } catch (const std::exception& error) {
            [request finishLoadingWithError:[NSError errorWithDomain:@"dev.haylen" code:1 userInfo:@{NSLocalizedDescriptionKey : [NSString stringWithUTF8String:error.what()]}]];
            return YES;
        }
    }
    [request finishLoading];
    return YES;
}

@end
