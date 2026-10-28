// Quickshell.Services.Mpris against tests/mpris/bin/media-control.
import QtQuick
import Quickshell
import Quickshell.Io
import Quickshell.Services.Mpris
import "../lib"

ShellRoot {
    property Check t: Check {}
    Component.onCompleted: Mpris.players
    property FileView art: FileView { printErrors: false; blockLoading: true }

    property Timer settle: Timer {
        interval: 2000; running: true
        onTriggered: {
            const p = Mpris.players.values[0];
            t.check("player", p !== undefined);
            t.check("diff applied", p?.trackTitle === "Next song", p?.trackTitle);
            t.check("artist kept across diff", p?.trackArtist === "Band", p?.trackArtist);
            t.check("art url", (p?.trackArtUrl ?? "").startsWith("file://"), p?.trackArtUrl);
            art.path = (p?.trackArtUrl ?? "").replace("file://", "");
            t.check("art written", art.text().startsWith("\x89PNG") || art.text().indexOf("PNG") === 1, art.path);
            t.finish();
        }
    }
}
