import SwiftUI

// The confirm screen in SwiftUI: the question with Confirm and Decline, which answer through the closure, and Close, which dismisses the screen through the dismiss action of SwiftUI without an answer, so the runtime ends the screen as `cancelled`.
struct NativeDemoConfirmView: View {
    let title: String
    let question: String
    let answered: @MainActor (Bool) -> Void

    @Environment(\.dismiss) private var dismiss

    var body: some View {
        VStack(spacing: 24) {
            Text(title)
                .font(.largeTitle)
                .bold()
            Text(question)
                .multilineTextAlignment(.center)
            HStack(spacing: 16) {
                Button("Close") {
                    dismiss()
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
