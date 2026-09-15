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
#include "player_utils.h"
#include "objectpath.h"

#ifdef QT_QUICK_LIB
#include <QQuickItem>
#include <QQuickWindow>
#include <QWindow>
#endif

#ifdef QT_QUICK_LIB
class QuickItemLocatorContext : public ObjectLocatorContext {
public:
    QuickItemLocatorContext(Player * player,
                            const QtJson::JsonObject & command,
                            const QString & objKey);
    QQuickItem * item;
    QQuickWindow * window;
};
#endif

#ifdef QT_QUICK_LIB
QuickItemLocatorContext::QuickItemLocatorContext(
    Player * player, const QtJson::JsonObject & command, const QString & objKey)
    : ObjectLocatorContext(player, command, objKey) {
    if (!hasError()) {
        item = qobject_cast<QQuickItem *>(obj);
        if (!item) {
            lastError = player->createError(
                "NotAWidget",
                QString::fromUtf8("Object (id:%1) is not a QQuickItem")
                    .arg(id));
        } else {
            window = item->window();
            if (!window) {
                lastError = player->createError(
                    "NoWindowForQuickItem",
                    "No QQuickWindow associated to the item.");
            }
        }
    }
}
#endif

#ifdef QT_QUICK_LIB
void dump_quick_items(Player * player,
                      const QList<QQuickItem *> & items,
                      const qulonglong & viewid,
                      bool recursive,
                      QtJson::JsonObject & out) {
    QtJson::JsonArray outitems;
    foreach (QQuickItem * item, items) {
        QtJson::JsonObject outitem;
        qulonglong oid = player->registerObject(item);
        outitem["oid"] = oid;
        outitem["viewid"] = viewid;
        QObject * itemObject = dynamic_cast<QObject *>(item);
        if (itemObject) {
            const QMetaObject * mo = itemObject->metaObject();
            QStringList classes;
            while (mo) {
                classes << mo->className();
                mo = mo->superClass();
            }
            outitem["classes"] = classes;
            outitem["path"] = ObjectPath::objectPath(itemObject);
        }
        if (recursive) {
            dump_quick_items(player, item->childItems(), viewid, recursive,
                             outitem);
        }
        outitems << outitem;
    }
    out["items"] = outitems;
}
#endif

QtJson::JsonObject Player::quick_item_find(const QtJson::JsonObject & command) {
    QtJson::JsonObject result;
#ifdef QT_QUICK_LIB
    WidgetLocatorContext<QQuickWindow> ctx(this, command, "quick_window_oid");
    if (ctx.hasError()) {
        return ctx.lastError;
    }
    QQuickItem * item;
    qulonglong id;
    QString qid = command["qid"].toString();
    if (!qid.isEmpty()) {
        item = ObjectPath::findQuickItemById(ctx.widget->contentItem(), qid);
        id = registerObject(item);
        if (id == 0) {
            return createError(
                "InvalidQuickItem",
                QString("Unable to find quick item with id `%1`").arg(qid));
        }
    } else {
        QString path = command["path"].toString();
        item = ObjectPath::findQuickItem(ctx.widget, path);
        id = registerObject(item);
        if (id == 0) {
            return createError(
                "InvalidQuickItem",
                QString("Unable to find quick item with path `%1`").arg(path));
        }
    }
    result["oid"] = id;
    result["quick_window_oid"] = command["quick_window_oid"].toString();
    dump_object(item, result);
#else
    Q_UNUSED(command);
    result = createQtQuickOnlyError();
#endif
    return result;
}

QtJson::JsonObject Player::quick_items_find(
    const QtJson::JsonObject & command) {
    QtJson::JsonObject result;
#ifdef QT_QUICK_LIB
    WidgetLocatorContext<QQuickWindow> ctx(this, command, "quick_window_oid");
    if (ctx.hasError()) {
        return ctx.lastError;
    }

    QString path = command["path"].toString();
    QList<QQuickItem *> items = ObjectPath::findQuickItems(ctx.widget, path);
    if (items.isEmpty()) {
        return createError(
            "InvalidQuickItem",
            QString("Unable to find quick items with path `%1`").arg(path));
    }

    QtJson::JsonArray outItems;
    foreach (QQuickItem * item, items) {
        QtJson::JsonObject out;
        qulonglong id = registerObject(item);
        out["oid"] = id;
        dump_object(item, out);
        outItems << out;
    }
    result["items"] = outItems;
    result["quick_window_oid"] = command["quick_window_oid"].toString();
#else
    Q_UNUSED(command);
    result = createQtQuickOnlyError();
#endif
    return result;
}

#ifdef QT_QUICK_LIB
static QQuickItem * findQuickItemByProperty(QQuickItem * item,
                                             const QString & propertyName,
                                             const QString & propertyValue) {
    QVariant value = item->property(propertyName.toLatin1());
    if (value.isValid() && value.toString() == propertyValue) {
        return item;
    }
    foreach (QQuickItem * child, item->childItems()) {
        QQuickItem * found = findQuickItemByProperty(
            child, propertyName, propertyValue);
        if (found) {
            return found;
        }
    }
    return nullptr;
}
#endif

QtJson::JsonObject Player::quick_item_find_by_property(
    const QtJson::JsonObject & command) {
    QtJson::JsonObject result;
#ifdef QT_QUICK_LIB
    WidgetLocatorContext<QQuickWindow> ctx(this, command, "quick_window_oid");
    if (ctx.hasError()) {
        return ctx.lastError;
    }

    QString propertyName = command["property_name"].toString();
    QString propertyValue = command["property_value"].toString();
    QQuickItem * item = findQuickItemByProperty(
        ctx.widget->contentItem(), propertyName, propertyValue);
    if (!item) {
        return createError(
            "InvalidQuickItem",
            QString("Unable to find quick item with %1 == `%2`")
                .arg(propertyName, propertyValue));
    }

    qulonglong id = registerObject(item);
    result["oid"] = id;
    dump_object(item, result);
    result["quick_window_oid"] = command["quick_window_oid"].toString();
#else
    Q_UNUSED(command);
    result = createQtQuickOnlyError();
#endif
    return result;
}

QtJson::JsonObject Player::quick_item_click(
    const QtJson::JsonObject & command) {
#ifdef QT_QUICK_LIB
    QuickItemLocatorContext ctx(this, command, "oid");
    if (ctx.hasError()) {
        return ctx.lastError;
    }

    int x = command.contains("xpos") ? command["xpos"].toInt() : -1;
    int y = command.contains("ypos") ? command["ypos"].toInt() : -1;
    QPoint clickPoint;
    if (x >= 0 && y >= 0) {
        clickPoint = QPoint(x, y);
    } else {
        clickPoint = QPoint(ctx.item->width() / 2.0,
                            ctx.item->height() / 2.0);
    }

    QPoint scenePosition = ctx.item->mapToScene(clickPoint).toPoint();
    QString action = command["mouseAction"].toString();

    if (action == "doubleclick") {
        mouse_dclick(ctx.window, scenePosition);
    } else if (action == "rightclick") {
        mouse_click(ctx.window, scenePosition, Qt::RightButton);
    } else if (action == "middleclick") {
        mouse_click(ctx.window, scenePosition, Qt::MiddleButton);
    } else {
        mouse_click(ctx.window, scenePosition, Qt::LeftButton);
    }
    QtJson::JsonObject result;
    return result;
#else
    Q_UNUSED(command);
    return createQtQuickOnlyError();
#endif
}

QtJson::JsonObject Player::quick_item_key_click(
    const QtJson::JsonObject & command) {
#ifdef QT_QUICK_LIB
    QuickItemLocatorContext ctx(this, command, "oid");
    if (ctx.hasError()) {
        return ctx.lastError;
    }

    bool keyOk = false;
    QString keyString = command["key"].toString();
    Qt::Key key = static_cast<Qt::Key>(keyString.toUInt(&keyOk, 16));
    if (!keyOk || key == Qt::Key_unknown) {
        return createError(
            "Unknown key",
            QString::fromUtf8("Can't cast %1 to Qt::Key").arg(keyString));
    }

    Qt::KeyboardModifiers modifiers = Qt::NoModifier;
    QVariant modifiersValue = command["modifiers"];
    if (modifiersValue.type() == QVariant::List) {
        foreach (const QVariant & modifier, modifiersValue.toList()) {
            bool modifierOk = false;
            uint value = modifier.toString().toUInt(&modifierOk, 16);
            if (modifierOk) {
                modifiers |= static_cast<Qt::KeyboardModifier>(value);
            }
        }
    }

    key_click(ctx.window, key, modifiers);
    QtJson::JsonObject result;
    return result;
#else
    Q_UNUSED(command);
    return createQtQuickOnlyError();
#endif
}

QtJson::JsonObject Player::quick_item_key_press(
    const QtJson::JsonObject & command) {
#ifdef QT_QUICK_LIB
    QuickItemLocatorContext ctx(this, command, "oid");
    if (ctx.hasError()) {
        return ctx.lastError;
    }

    bool keyOk = false;
    QString keyString = command["key"].toString();
    Qt::Key key = static_cast<Qt::Key>(keyString.toUInt(&keyOk, 16));
    if (!keyOk || key == Qt::Key_unknown) {
        return createError(
            "Unknown key",
            QString::fromUtf8("Can't cast %1 to Qt::Key").arg(keyString));
    }

    Qt::KeyboardModifiers modifiers = Qt::NoModifier;
    QVariant modifiersValue = command["modifiers"];
    if (modifiersValue.type() == QVariant::List) {
        foreach (const QVariant & modifier, modifiersValue.toList()) {
            bool modifierOk = false;
            uint value = modifier.toString().toUInt(&modifierOk, 16);
            if (modifierOk) {
                modifiers |= static_cast<Qt::KeyboardModifier>(value);
            }
        }
    }

    int duration = command.contains("duration")
        ? command["duration"].toInt()
        : 800;
    key_press(ctx.window, key, modifiers, duration);
    QtJson::JsonObject result;
    return result;
#else
    Q_UNUSED(command);
    return createQtQuickOnlyError();
#endif
}

QtJson::JsonObject Player::quick_item_children(
    const QtJson::JsonObject & command) {
    QtJson::JsonObject result;
#ifdef QT_QUICK_LIB
    QuickItemLocatorContext ctx(this, command, "oid");
    if (ctx.hasError()) {
        return ctx.lastError;
    }

    bool recursive = command["recursive"].toBool();
    dump_quick_items(this, ctx.item->childItems(), ctx.id, recursive, result);
#else
    Q_UNUSED(command);
    result = createQtQuickOnlyError();
#endif
    return result;
}
