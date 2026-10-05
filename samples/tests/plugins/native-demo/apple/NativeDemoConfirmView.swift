import SwiftUI

// The confirm screen in SwiftUI: the question with Confirm and Decline, which answer through `answered`, and Close, which leaves without an answer through `closed`.
struct NativeDemoConfirmView: View {
    let title: String
    let question: String
    let answered: @MainActor (Bool) -> Void
    let closed: @MainActor () -> Void

    var body: some View {
        VStack(spacing: 24) {
            Text(title)
                .font(.largeTitle)
                .bold()
            Text(question)
                .multilineTextAlignment(.center)
            HStack(spacing: 16) {
                Button("Close") {
                    closed()
                }
                Button("Decline") {
                    answered(false)
                }
                Button("Confirm") {
                    answered(true)
                }
                .buttonStyle(.borderedProminent)
            }
        }
        .padding(40)
        .frame(minWidth: 460, minHeight: 260)
    }
}
