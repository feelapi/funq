/*
Copyright: SCLE SFE
Contributor: Julien Pagès <j.parkouss@gmail.com>

This software is a computer program whose purpose is to test graphical
applications written with the QT framework (http://qt.digia.com/).

This software is governed by the CeCILL v2.1 license under French law and
abiding by the rules of distribution of free software.  You can  use,
modify and/ or redistribute the software under the terms of the CeCILL
license as circulated by CEA, CNRS and INRIA at the following URL
"http://www.cecill.info".

As a counterpart to the access to the source code and  rights to copy,
modify and redistribute granted by the license, users are provided only
with a limited warranty  and the software's author,  the holder of the
economic rights,  and the successive licensors  have only  limited
liability.

In this respect, the user's attention is drawn to the risks associated
with loading,  using,  modifying and/or developing or reproducing the
software by the user in light of its specific status of free software,
that may mean  that it is complicated to manipulate,  and  that  also
therefore means  that it is reserved for developers  and  experienced
professionals having in-depth computer knowledge. Users are therefore
encouraged to load and test the software's suitability as regards their
requirements in conditions enabling the security of their systems and/or
data to be ensured and,  more generally, to use and operate it in the
same conditions as regards security.

The fact that you are presently reading this means that you have had
knowledge of the CeCILL v2.1 license and that you accept its terms.
*/

#include "player.h"

#include "dragndropresponse.h"
#include "objectpath.h"
#include "player_utils.h"
#include "shortcutresponse.h"

#include <QAction>
#include <QApplication>
#include <QBuffer>
#include <QComboBox>
#include <QGraphicsView>
#include <QHeaderView>
#include <QMetaMethod>
#include <QScreen>
#include <QStringList>
#include <QTableView>
#include <QTimer>
#include <QTreeView>
#include <QWidget>
#include <QWindow>

using namespace ObjectPath;

Player::Player(QIODevice * device, QObject * parent)
    : JsonClient(device, parent) {
}

qulonglong Player::registerObject(QObject * object) {
    if (!object) {
        return 0;
    }
    qulonglong id = (qulonglong)object;
    if (!m_registeredObjects.contains(id)) {
        connect(object, SIGNAL(destroyed(QObject *)), this,
                SLOT(objectDeleted(QObject *)));
        m_registeredObjects[id] = object;
    }
    return id;
}

QObject * Player::registeredObject(const qulonglong & id) {
    return m_registeredObjects[id];
}

void Player::objectDeleted(QObject * object) {
    qulonglong id = (qulonglong)object;
    m_registeredObjects.remove(id);
}

QtJson::JsonObject Player::list_commands(const QtJson::JsonObject &) {
    const QMetaObject * metaObject = this->metaObject();
    QStringList methods;
    for (int i = metaObject->methodOffset(); i < metaObject->methodCount();
         ++i) {
        QMetaMethod method = metaObject->method(i);
        if (method.methodType() == QMetaMethod::Slot) {
            methods << QString::fromLatin1(
                metaObject->method(i).methodSignature());
        }
    }
    QtJson::JsonObject result;
    result["commands"] = methods;
    return result;
}

QtJson::JsonObject Player::widget_by_path(const QtJson::JsonObject & command) {
    QString path = command["path"].toString();
    QObject * o = findObject(path);
    qulonglong id = registerObject(o);
    if (id == 0) {
        return createError(
            "InvalidWidgetPath",
            QString("Unable to find widget with path `%1`").arg(path));
    }
    QtJson::JsonObject result;
    result["oid"] = id;
    dump_object(o, result);
    return result;
}

QtJson::JsonObject Player::active_widget(const QtJson::JsonObject & command) {
    QObject * active;
    QString type = command["type"].toString();
    if (type == "modal") {
        active = QApplication::activeModalWidget();
        if (!active) {
            active = QApplication::modalWindow();
        }
    } else if (type == "popup") {
        active = QApplication::activePopupWidget();
    } else if (type == "focus") {
        active = QApplication::focusWidget();
        if (!active) {
            active = QApplication::focusWindow();
        }
    } else {
        active = QApplication::activeWindow();
        if (!active) {
            QWindowList lst = QGuiApplication::topLevelWindows();
            if (!lst.isEmpty()) {
                active = lst.first();
            }
        }
    }
    if (!active) {
        return createError(
            "NoActiveWindow",
            QString::fromUtf8("There is no active widget (%1)").arg(type));
    }

    if (QWindow * window = qobject_cast<QWindow *>(active)) {
        window->requestActivate();
    } else if (QWidget * widget = qobject_cast<QWidget *>(active)) {
        widget->activateWindow();
    }

    qulonglong id = registerObject(active);
    QtJson::JsonObject result;
    result["oid"] = id;
    dump_object(active, result);
    return result;
}

QtJson::JsonObject Player::object_properties(
    const QtJson::JsonObject & command) {
    ObjectLocatorContext ctx(this, command, "oid");
    if (ctx.hasError()) {
        return ctx.lastError;
    }
    QtJson::JsonObject result;
    dump_properties(ctx.obj, result);
    return result;
}

QtJson::JsonObject Player::object_set_properties(
    const QtJson::JsonObject & command) {
    ObjectLocatorContext ctx(this, command, "oid");
    if (ctx.hasError()) {
        return ctx.lastError;
    }
    QVariantMap properties = command["properties"].value<QVariantMap>();
    _object_set_properties(ctx.obj, properties);
    QtJson::JsonObject result;
    return result;
}

void Player::_object_set_properties(QObject * object,
                                    const QVariantMap & properties) {
    for (QtJson::JsonObject::const_iterator iter = properties.begin();
         iter != properties.end(); ++iter) {
        object->setProperty(iter.key().toStdString().c_str(), iter.value());
    }
}

void recursive_list_widget(QWidget * widget, QtJson::JsonObject & out,
                           bool with_properties, bool recursive) {
    QtJson::JsonObject resultWidgets, resultWidget;
    dump_object(widget, resultWidget, with_properties);
    foreach (QObject * obj, widget->children()) {
        QWidget * subWidget = qobject_cast<QWidget *>(obj);
        if (recursive && subWidget) {
            recursive_list_widget(subWidget, resultWidgets, with_properties, recursive);
        }
    }
    resultWidget["children"] = resultWidgets;
    out[objectName(widget)] = resultWidget;
}

QtJson::JsonObject Player::widgets_list(const QtJson::JsonObject & command) {
    bool with_properties = command["with_properties"].toBool();
    bool recursive = command["recursive"].toBool();
    QtJson::JsonObject result;
    if (command.contains("oid")) {
        ObjectLocatorContext ctx(this, command, "oid");
        if (ctx.hasError()) {
            return ctx.lastError;
        }
        foreach (QObject * obj, ctx.obj->children()) {
            QWidget * subWidget = qobject_cast<QWidget *>(obj);
            if (subWidget) {
                recursive_list_widget(subWidget, result, with_properties, recursive);
            }
        }
    } else {
        QList<QWidget *> widgets = QApplication::topLevelWidgets();
        if (!widgets.isEmpty()) {
            foreach (QWidget * widget, widgets) {
                recursive_list_widget(widget, result, with_properties, recursive);
            }
        } else {
            // no qwidgets, this is probably a qtquick app - anyway, check for
            // windows
            foreach (QWindow * window, QApplication::topLevelWindows()) {
                QtJson::JsonObject resultWindow;
                dump_object(window, resultWindow, with_properties);
                result[resultWindow["path"].toString()] = resultWindow;
            }
        }
    }
    return result;
}

QtJson::JsonObject Player::quit(const QtJson::JsonObject &) {
    if (qApp) {
        qApp->exit();
    }
    QtJson::JsonObject result;
    return result;
}

QtJson::JsonObject Player::action_trigger(const QtJson::JsonObject & command) {
    WidgetLocatorContext<QAction> ctx(this, command, "oid");
    if (ctx.hasError()) {
        return ctx.lastError;
    }
    bool blocking = command["blocking"].toBool();
    if (blocking) {
        // block until QAction::trigger() returns
        ctx.widget->trigger();
    } else {
        // trigger the action, but return immediately
        QTimer::singleShot(0, ctx.widget, SLOT(trigger()));
    }
    QtJson::JsonObject result;
    return result;
}

QtJson::JsonObject Player::widget_click(const QtJson::JsonObject & command) {
    WidgetLocatorContext<QWidget> ctx(this, command, "oid");
    if (ctx.hasError()) {
        return ctx.lastError;
    }
    QString action = command["mouseAction"].toString();
    const auto click = [widget = ctx.widget, action] {
        const QPoint pos = widget->rect().center();
        if (action == "doubleclick") {
            mouse_dclick(widget, pos);
        } else if (action == "rightclick") {
            mouse_click(widget, pos, Qt::RightButton);
        } else if (action == "middleclick") {
            mouse_click(widget, pos, Qt::MiddleButton);
        } else {
            mouse_click(widget, pos, Qt::LeftButton);
        }
    };
    if (command.contains("blocking") && !command["blocking"].toBool())
        QTimer::singleShot(0, ctx.widget, click);
    else
        click();
    QtJson::JsonObject result;
    return result;
}

QtJson::JsonObject Player::widget_move(const QtJson::JsonObject & command) {
  WidgetLocatorContext<QWidget> ctx(this, command, "oid");
  if (ctx.hasError()) {
      return ctx.lastError;
  }

  QPoint pos = ctx.widget->pos();
  if (!command["x"].isNull()) {
    pos.setX(command["x"].toInt());
  }
  if (!command["y"].isNull()) {
    pos.setY(command["y"].toInt());
  }
  ctx.widget->move(pos);

  QtJson::JsonObject result;
  result["x"] = ctx.widget->x();
  result["y"] = ctx.widget->y();
  return result;
}

QtJson::JsonObject Player::widget_resize(const QtJson::JsonObject & command) {
  WidgetLocatorContext<QWidget> ctx(this, command, "oid");
  if (ctx.hasError()) {
      return ctx.lastError;
  }

  QSize size = ctx.widget->size();
  if (!command["width"].isNull()) {
    size.setWidth(command["width"].toInt());
  }
  if (!command["height"].isNull()) {
    size.setHeight(command["height"].toInt());
  }
  ctx.widget->resize(size);

  QtJson::JsonObject result;
  result["width"] = ctx.widget->width();
  result["height"] = ctx.widget->height();
  return result;
}

QtJson::JsonObject Player::widget_close(const QtJson::JsonObject & command) {
    WidgetLocatorContext<QWidget> ctx(this, command, "oid");
    if (ctx.hasError()) {
        return ctx.lastError;
    }

    QTimer::singleShot(0, ctx.widget, SLOT(close()));

    QtJson::JsonObject result;
    return result;
}

QtJson::JsonObject Player::widget_map_position(
    const QtJson::JsonObject & command) {
    WidgetLocatorContext<QWidget> ctx(this, command, "oid");
    if (ctx.hasError()) {
        return ctx.lastError;
    }
    QWidget * parent = 0;
    if (!command["parent_oid"].isNull()) {
        WidgetLocatorContext<QWidget> parentCtx(this, command, "parent_oid");
        if (parentCtx.hasError()) {
            return ctx.lastError;
        } else {
            parent = parentCtx.widget;
        }
    }
    QString direction = command["direction"].toString();
    QPoint pos;
    pos.setX(command["x"].toInt());
    pos.setY(command["y"].toInt());

    if (direction == "from") {
        if (parent) {
            pos = ctx.widget->mapFrom(parent, pos);
        } else {
            pos = ctx.widget->mapFromGlobal(pos);
        }
    } else if (direction == "to") {
        if (parent) {
            pos = ctx.widget->mapTo(parent, pos);
        } else {
            pos = ctx.widget->mapToGlobal(pos);
        }
    } else {
        return createError(
            "InvalidDirection",
            QString::fromUtf8("The direction '%1' is invalid").arg(direction));
    }

    QtJson::JsonObject result;
    result["x"] = pos.x();
    result["y"] = pos.y();
    return result;
}

QtJson::JsonObject Player::grab(const QtJson::JsonObject & command) {
    QPixmap pixmap;
    if (command.contains("oid")) {
        // grab a single widget
        WidgetLocatorContext<QWidget> ctx(this, command, "oid");
        if (ctx.hasError()) {
            return ctx.lastError;
        }
#if QT_VERSION_MAJOR >= 6
        pixmap = ctx.widget->grab();
#else
        pixmap = QPixmap::grabWidget(ctx.widget);
#endif
    } else {
        // grab the whole screen
#if QT_VERSION_MAJOR >= 6
        if (QScreen* screen = QGuiApplication::primaryScreen()) {
            pixmap = screen->grabWindow();
        }
#else
        pixmap = QPixmap::grabWindow(QApplication::desktop()->winId());
#endif
    }
    QString format = command["format"].toString();
    if (format.isEmpty()) {
        format = "PNG";
    }

    QBuffer buffer;
    pixmap.save(&buffer, "PNG");

    QtJson::JsonObject result;
    result["format"] = format;
    result["data"] = buffer.data().toBase64();
    return result;
}

QtJson::JsonObject Player::widget_keyclick(const QtJson::JsonObject & command) {
    QWidget * widget;
    if (command.contains("oid")) {
        WidgetLocatorContext<QWidget> ctx(this, command, "oid");
        if (ctx.hasError()) {
            return ctx.lastError;
        }
        widget = ctx.widget;
    } else {
        widget = qApp->activeWindow();
    }
    QString text = command["text"].toString();
    for (int i = 0; i < text.count(); ++i) {
        QChar ch = text[i];
        int key = (int)ch.toLatin1();
        qApp->postEvent(
            widget,
            new QKeyEvent(QKeyEvent::KeyPress, key, Qt::NoModifier, ch));
        qApp->postEvent(
            widget,
            new QKeyEvent(QKeyEvent::KeyRelease, key, Qt::NoModifier, ch));
    }
    QtJson::JsonObject result;
    return result;
}

DelayedResponse * Player::shortcut(const QtJson::JsonObject & command) {
    return new ShortcutResponse(this, command);
}

QtJson::JsonObject Player::tabbar_list(const QtJson::JsonObject & command) {
    WidgetLocatorContext<QTabBar> ctx(this, command, "oid");
    if (ctx.hasError()) {
        return ctx.lastError;
    }
    QStringList texts;
    for (int i = 0; i < ctx.widget->count(); ++i) {
        texts << ctx.widget->tabText(i);
    }
    QtJson::JsonObject result;
    result["tabtexts"] = texts;
    return result;
}

DelayedResponse * Player::drag_n_drop(const QtJson::JsonObject & command) {
    return new DragNDropResponse(this, command);
}

QtJson::JsonObject Player::call_slot(const QtJson::JsonObject & command) {
    WidgetLocatorContext<QWidget> ctx(this, command, "oid");
    if (ctx.hasError()) {
        return ctx.lastError;
    }
    QString slot_name = command["slot_name"].toString();
    QVariant result_slot;
    bool invokedMeth = QMetaObject::invokeMethod(
        ctx.widget, slot_name.toLocal8Bit().data(), Qt::DirectConnection,
        Q_RETURN_ARG(QVariant, result_slot),
        Q_ARG(QVariant, command["params"]));
    if (!invokedMeth) {
        return createError("NoMethodInvoked",
                           QString::fromUtf8("The slot %1 could not be called")
                               .arg(slot_name));
    }

    QtJson::JsonObject result;
    result["result_slot"] = result_slot;
    return result;
}

QtJson::JsonObject Player::widget_activate_focus(
    const QtJson::JsonObject & command) {
    WidgetLocatorContext<QWidget> ctx(this, command, "oid");
    if (ctx.hasError()) {
        return ctx.lastError;
    }
    activate_focus(ctx.widget);

    QtJson::JsonObject result;
    return result;
}
