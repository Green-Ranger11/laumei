// Region input masks: item rects, child regions and their intersection modes.
import QtQuick
import Quickshell
import "../lib"

ShellRoot {
    id: root
    property Check t: Check {}

    PanelWindow {
        id: win
        implicitWidth: 200
        implicitHeight: 100
        color: "transparent"
        Rectangle { id: box; x: 10; y: 10; width: 50; height: 50 }
        mask: Region { item: box }
    }

    property Region empty: Region {}
    property Region holed: Region {
        width: 100; height: 100
        Region { x: 40; y: 40; width: 20; height: 20; intersection: Intersection.Subtract }
        Region { x: 150; y: 0; width: 10; height: 10 }
    }
    property Region overlap: Region {
        width: 100; height: 100
        Region { x: 50; y: 50; width: 100; height: 100; intersection: Intersection.Intersect }
    }
    property Region round: Region { width: 100; height: 100; shape: RegionShape.Ellipse }

    Component.onCompleted: {
        t.check("item rect inside", QsMacWindow.maskContains(win.mask, 20, 20));
        t.check("item rect outside", !QsMacWindow.maskContains(win.mask, 100, 50));
        t.check("empty region", !QsMacWindow.maskContains(empty, 0, 0));
        t.check("subtract hole", !QsMacWindow.maskContains(holed, 50, 50));
        t.check("subtract keeps rest", QsMacWindow.maskContains(holed, 10, 10));
        t.check("combine adds", QsMacWindow.maskContains(holed, 155, 5));
        t.check("intersect", QsMacWindow.maskContains(overlap, 75, 75) && !QsMacWindow.maskContains(overlap, 25, 25));
        t.check("ellipse corner", !QsMacWindow.maskContains(round, 2, 2) && QsMacWindow.maskContains(round, 50, 50));
        t.finish();
    }
}
