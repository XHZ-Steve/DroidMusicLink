#pragma once
#include <QQuickItem>
#include <QQuickWindow>
#include <QList>
inline QList<QQuickItem*> visualItemsNamed(QQuickWindow *window,const QString &name){
 QList<QQuickItem*> result,pending{window->contentItem()};
 while(!pending.isEmpty()){auto item=pending.takeLast();if(item->objectName()==name)result<<item;pending.append(item->childItems());}
 return result;
}
