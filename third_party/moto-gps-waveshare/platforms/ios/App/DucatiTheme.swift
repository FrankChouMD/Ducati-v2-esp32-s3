import SwiftUI

// Ducati V2 personal adaptation, 2026-10-16. The MOTO GPS attribution and
// navigation implementation remain in their original sources and notices.
enum DucatiTheme {
    static let background = Color(hex: 0x000000)
    static let surface = Color(hex: 0x121214)
    static let raised = Color(hex: 0x1C1C1F)
    static let border = Color(hex: 0x303034)
    static let accent = Color(hex: 0xE80016)
    static let primary = Color(hex: 0xFFFFFF)
    static let secondary = Color(hex: 0xA3A3AA)
    static let ready = Color(hex: 0xA6D800)
    static let warning = Color(hex: 0xFFC400)

    static func label(_ size: CGFloat = 14) -> Font {
        .custom("BarlowCondensed-Medium", size: size, relativeTo: .caption)
    }

    static func number(_ size: CGFloat = 56) -> Font {
        .custom("BarlowCondensed-SemiBoldItalic", size: size, relativeTo: .largeTitle)
    }
}

private extension Color {
    init(hex: UInt32) {
        self.init(.sRGB, red: Double((hex >> 16) & 255) / 255,
                  green: Double((hex >> 8) & 255) / 255,
                  blue: Double(hex & 255) / 255, opacity: 1)
    }
}

private struct DucatiScreenStyle: ViewModifier {
    func body(content: Content) -> some View {
        content
            .foregroundStyle(DucatiTheme.primary)
            .background(DucatiTheme.background)
            .toolbarBackground(DucatiTheme.background, for: .navigationBar)
            .toolbarBackground(.visible, for: .navigationBar)
            .toolbarColorScheme(.dark, for: .navigationBar)
    }
}

extension View {
    func ducatiScreen() -> some View { modifier(DucatiScreenStyle()) }

    func ducatiAppTheme() -> some View {
        tint(DucatiTheme.accent).preferredColorScheme(.dark)
    }

    func ducatiCard(selected: Bool = false) -> some View {
        padding(18)
            .background(selected ? DucatiTheme.accent.opacity(0.09) : DucatiTheme.surface,
                        in: RoundedRectangle(cornerRadius: 18))
            .overlay {
                RoundedRectangle(cornerRadius: 18)
                    .strokeBorder(selected ? DucatiTheme.accent : DucatiTheme.border, lineWidth: 1)
            }
    }

    func ducatiList() -> some View {
        listStyle(.insetGrouped)
            .scrollContentBackground(.hidden)
            .environment(\.defaultMinListRowHeight, 52)
            .ducatiScreen()
    }
}

struct DucatiBrand: View {
    var caption = "PHONE COMPANION"

    var body: some View {
        HStack(spacing: 10) {
            RoundedRectangle(cornerRadius: 1)
                .fill(DucatiTheme.accent)
                .frame(width: 4, height: 27)
            VStack(alignment: .leading, spacing: 2) {
                Text("PANIGALE V2S")
                    .font(DucatiTheme.label(18)).tracking(2.2)
                    .foregroundStyle(DucatiTheme.primary)
                Text(caption)
                    .font(DucatiTheme.label(10)).tracking(2)
                    .foregroundStyle(DucatiTheme.secondary)
            }
        }
        .accessibilityElement(children: .combine)
    }
}

struct DucatiSectionTitle: View {
    let title: String
    let index: String

    var body: some View {
        HStack(alignment: .firstTextBaseline) {
            Text(title).font(.subheadline.weight(.semibold))
            Spacer()
            Text(index).font(DucatiTheme.label(13)).tracking(1.6)
                .foregroundStyle(DucatiTheme.secondary)
        }
        .foregroundStyle(DucatiTheme.primary)
        .padding(.top, 4)
        .accessibilityElement(children: .ignore)
        .accessibilityLabel(title)
    }
}

struct DucatiStatus: View {
    let title: String
    var color = DucatiTheme.secondary

    var body: some View {
        HStack(spacing: 6) {
            Circle().fill(color).frame(width: 6, height: 6).accessibilityHidden(true)
            Text(title).font(.caption.weight(.medium)).foregroundStyle(color)
        }
        .padding(.horizontal, 10).padding(.vertical, 7)
        .background(color.opacity(0.1), in: Capsule())
        .fixedSize(horizontal: false, vertical: true)
    }
}

/// Keep the instrument number dominant while units remain readable. This
/// accepts the model's complete formatted value, including hours and minutes.
struct DucatiReadout: View {
    let value: String
    var size: CGFloat = 56
    var unitSize: CGFloat = 16

    private var formatted: Text {
        value.reduce(Text("")) { result, character in
            let isNumber = character.isNumber || character == "." || character == "—"
            return result + Text(String(character))
                .font(isNumber ? DucatiTheme.number(size) : DucatiTheme.label(unitSize))
                .foregroundColor(isNumber ? DucatiTheme.primary : DucatiTheme.secondary)
        }
    }

    var body: some View {
        formatted.monospacedDigit().fixedSize(horizontal: false, vertical: true)
    }
}

struct DucatiPrimaryButtonStyle: ButtonStyle {
    @Environment(\.isEnabled) private var isEnabled

    func makeBody(configuration: Configuration) -> some View {
        configuration.label
            .font(.headline)
            .foregroundStyle(DucatiTheme.primary)
            .frame(maxWidth: .infinity, minHeight: 54)
            .background(isEnabled ? DucatiTheme.accent : DucatiTheme.raised,
                        in: RoundedRectangle(cornerRadius: 16))
            .opacity(configuration.isPressed ? 0.75 : (isEnabled ? 1 : 0.5))
    }
}

struct DucatiToolTile: View {
    let title: String
    let subtitle: String
    let symbol: String

    var body: some View {
        VStack(alignment: .leading, spacing: 14) {
            Image(systemName: symbol).font(.system(size: 23, weight: .light))
                .foregroundStyle(DucatiTheme.accent).accessibilityHidden(true)
            VStack(alignment: .leading, spacing: 5) {
                Text(title).font(.subheadline.weight(.semibold)).foregroundStyle(DucatiTheme.primary)
                Text(subtitle).font(.caption).foregroundStyle(DucatiTheme.secondary)
            }
            .fixedSize(horizontal: false, vertical: true)
        }
        .frame(maxWidth: .infinity, maxHeight: .infinity, alignment: .leading)
        .ducatiCard()
        .contentShape(RoundedRectangle(cornerRadius: 18))
    }
}
