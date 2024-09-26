#include <QGraphicsScene>
#include <QGraphicsSceneMouseEvent>
#include <QGraphicsLineItem>
#include <QMouseEvent>
#include <QLineF>
#include <QVector>
#include <QDebug>

class MyGraphicsScene : public QGraphicsScene {
    Q_OBJECT

public:
    MyGraphicsScene(QObject* parent = nullptr) : QGraphicsScene(parent) {}

    void startDrawingLine() {
        drawingLine = true;
    }

    void stopDrawingLine() {
        drawingLine = false;
        if (currentLineItem) {
            double length = currentLineItem->line().length();
            emit addLineInfo(currentLineItem, length);
            // startPoint = currentLineItem->line().p1();
            // endPoint = currentLineItem->line().p2();

            lines.append(currentLineItem);
            currentLineItem = nullptr;
        }
    }

    void removeSelectedLine() {
        QList<QGraphicsItem*> selectedItemsList = selectedItems();
        if (selectedItemsList.isEmpty()) {
            qDebug() << "No items selected.";
            return;
        }

        QGraphicsLineItem* selectedLine = qgraphicsitem_cast<QGraphicsLineItem*>(selectedItems().first());
        if (selectedLine) {
            removeLine(selectedLine);
        }
    }

signals:
    void addLineInfo(QGraphicsLineItem* lineItem, double length);

protected:
    void mousePressEvent(QGraphicsSceneMouseEvent* event) override {
        if (drawingLine) {
            startDrawingLineAt(event->scenePos());
        } else {
            QGraphicsScene::mousePressEvent(event);
        }
    }

    void mouseMoveEvent(QGraphicsSceneMouseEvent* event) override {
        if (drawingLine && currentLineItem) {
            updateLineEnd(event->scenePos());
        } else {
            QGraphicsScene::mouseMoveEvent(event);
        }
    }

    void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) override {
        if (drawingLine) {
            stopDrawingLine();
        } else {
            QGraphicsScene::mouseReleaseEvent(event);
        }
    }

public:
    void startDrawingLineAt(const QPointF& startPoint) {
        currentLineItem = new QGraphicsLineItem(QLineF(startPoint, startPoint));
        currentLineItem->setFlag(QGraphicsItem::ItemIsSelectable);
        addItem(currentLineItem);
    }

    void updateLineEnd(const QPointF& endPoint) {
        if (currentLineItem) {
            currentLineItem->setLine(QLineF(currentLineItem->line().p1(), endPoint));
        }
    }

    void removeLine(QGraphicsLineItem* lineItem) {
        if (lines.contains(lineItem)) {
            removeItem(lineItem);
            delete lineItem;
            lines.removeAll(lineItem);
        }
    }

    QVector<QGraphicsLineItem*> lines;
    QGraphicsLineItem* currentLineItem = nullptr;
    bool drawingLine = false;
};
