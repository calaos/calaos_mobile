import QtQuick
import "../src/layout.mjs" as Layout
import "../assets/variants.mjs" as Assets

// Calaos background at any size. Place it at integer coordinates: dot
// positions are computed in item pixels.
ShaderEffect {
    id: root

    readonly property var variant: Layout.pickVariant(Assets.variants, width, height)
    readonly property string variantName: variant === Assets.variants.phone ? "phone" : "desktop"
    readonly property var boxes: Layout.layout(variant, width, height)
    readonly property url assets: Qt.resolvedUrl("../assets/" + variantName + "/")

    property vector2d itemSize: Qt.vector2d(width, height)
    property vector2d pitch: Qt.vector2d(variant.pitch[0], variant.pitch[1])
    property vector2d dotSize: Qt.vector2d(variant.grids.a.size[0], variant.grids.a.size[1])
    property color dotColor: variant.dotColor
    property vector4d boxA: Qt.vector4d(boxes.a.x, boxes.a.y, boxes.a.width, boxes.a.height)
    property vector4d boxB: Qt.vector4d(boxes.b.x, boxes.b.y, boxes.b.width, boxes.b.height)
    property vector2d dotOpacity: Qt.vector2d(boxes.a.opacity, boxes.b.opacity)

    property Image base: Image { source: root.assets + "base.png"; visible: false; smooth: true }
    property Image maskA: Image { source: root.assets + "mask-a.png"; visible: false; smooth: true }
    property Image maskB: Image { source: root.assets + "mask-b.png"; visible: false; smooth: true }

    readonly property bool ready: base.status === Image.Ready
                                  && maskA.status === Image.Ready
                                  && maskB.status === Image.Ready

    fragmentShader: Qt.resolvedUrl("background.frag.qsb")
}
