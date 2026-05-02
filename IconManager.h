#ifndef ICONMANAGER_H
#define ICONMANAGER_H

#include <QIcon>
#include <QMap>

class IconManager
{
public:
    // Obtener la única instancia (Singleton)
    static IconManager& instance() {
        static IconManager instance;
        return instance;
    }

    // Obtener un icono por su nombre
    QIcon getIcon(const QString& name) {
        if (!icons.contains(name)) {
            if (name == "play"){
                icons[name] = QIcon::fromTheme("media-playback-start", QIcon(":/iconos/icono_" + name + ".png"));
            } else if (name == "pause") {
                icons[name] = QIcon::fromTheme("media-playback-pause", QIcon(":/iconos/icono_" + name + ".png"));
            } else if (name == "pause") {
                icons[name] = QIcon::fromTheme("media-playback-pause", QIcon(":/iconos/icono_" + name + ".png"));
            } else if (name == "stop") {
                icons[name] = QIcon::fromTheme("media-playback-stop", QIcon(":/iconos/icono_" + name + ".png"));
            } else if (name == "backward") {
                icons[name] = QIcon::fromTheme("media-skip-backward", QIcon(":/iconos/icono_" + name + ".png"));
            } else if (name == "forward") {
                icons[name] = QIcon::fromTheme("media-skip-forward", QIcon(":/iconos/icono_" + name + ".png"));
            } else if (name == "record") {
                icons[name] = QIcon::fromTheme("media-record", QIcon(":/iconos/icono_" + name + ".png"));
            } else if (name == "eject") {
                icons[name] = QIcon::fromTheme("media-eject", QIcon(":/iconos/icono_" + name + ".png"));
            } else if (name == "repeat") {
                icons[name] = QIcon::fromTheme("media-playlist-repeat", QIcon(":/iconos/icono_" + name + ".png"));
            } else if (name == "shuffle") {
                icons[name] = QIcon::fromTheme("media-playlist-shuffle", QIcon(":/iconos/icono_" + name + ".png"));
            } else if (name == "open") {
                icons[name] = QIcon::fromTheme("document-open", QIcon(":/iconos/icono_" + name + ".png"));
            } else if (name == "save") {
                icons[name] = QIcon::fromTheme("document-save", QIcon(":/iconos/icono_" + name + ".png"));
            } else if (name == "save as") {
                icons[name] = QIcon::fromTheme("document-save-as", QIcon(":/iconos/icono_" + name + ".png"));
            } else if (name == "new") {
                icons[name] = QIcon::fromTheme("document-new", QIcon(":/iconos/icono_" + name + ".png"));
            } else if (name == "camera") {
                icons[name] = QIcon::fromTheme("camera-video", QIcon(":/iconos/icono_" + name + ".png"));
            } else if (name == "player") {
                icons[name] = QIcon::fromTheme("user-available", QIcon(":/iconos/icono_" + name + ".png"));
            } else if (name == "disc") {
                icons[name] = QIcon::fromTheme("media-optical", QIcon(":/iconos/icono_" + name + ".png"));
            } else {
            // Cargar el icono si no existe
                icons[name] = QIcon(":/iconos/icono_" + name + ".png");
            }
        }
        return icons[name];
    }

private:
    IconManager() {}  // Constructor privado
    ~IconManager() {}
    IconManager(const IconManager&) = delete;
    IconManager& operator=(const IconManager&) = delete;

    QMap<QString, QIcon> icons;  // Cache de iconos
};

#endif // ICONMANAGER_H
