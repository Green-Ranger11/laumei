import QtQml

// One delegate instance per `model` entry, each given `modelData`.
// Instantiator already has `model` and a default `delegate` property.
Instantiator {
    readonly property var instances: {
        const out = [];
        for (let i = 0; i < count; i++) out.push(objectAt(i));
        return out;
    }
}
