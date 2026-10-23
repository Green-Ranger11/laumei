import QtQuick

// Square icon sized by `implicitSize`, rendered at that size.
Image {
    property real implicitSize: 0

    width: implicitSize
    height: implicitSize
    sourceSize.width: implicitSize * Screen.devicePixelRatio
    sourceSize.height: implicitSize * Screen.devicePixelRatio
    fillMode: Image.PreserveAspectFit
    smooth: true
    mipmap: true
}
