#include <QGraphicsView>
#include <QGraphicsScene>
#include <QGraphicsRectItem>
#include <QWidget>
#include <QMouseEvent>
#include <QGraphicsSceneMouseEvent>
#include <QDebug>

class RectItem : public QGraphicsRectItem {
public:
    explicit RectItem(QGraphicsItem* parent = nullptr)
        : QGraphicsRectItem(parent), resizing(false), moving(false) {
        setFlags(QGraphicsItem::ItemIsSelectable | QGraphicsItem::ItemIsMovable);
    }

    void setFrameSize(const QSizeF& size) {
        frameSize = size;
    }

    QRectF getRectInPercentage() const {
        if (frameSize.isEmpty()) return QRectF();

        QRectF rect = this->rect();
        qDebug() << rect.x();
        qDebug() << rect.y();
        qDebug() << rect.width();
        qDebug() << rect.height();
        qDebug() << frameSize.width();
        qDebug() << frameSize.height();
        qreal xPerc = rect.x() / frameSize.width();
        qreal yPerc = rect.y() / frameSize.height();
        qreal widthPerc = rect.width() / frameSize.width();
        qreal heightPerc = rect.height() / frameSize.height();
        
        return QRectF(xPerc, yPerc, widthPerc, heightPerc);
    }

protected:
    void mousePressEvent(QGraphicsSceneMouseEvent* event) override {
        if (event->button() == Qt::LeftButton) {
            if (isResizingCorner(event->pos())) {
                resizing = true;
                startPos = event->pos();
            } else {
                moving = true;
                startPos = event->pos();
            }
            // QGraphicsRectItem::mousePressEvent(event);
        }
    }

    void mouseMoveEvent(QGraphicsSceneMouseEvent* event) override {
        if (resizing) {
            QPointF diff = event->pos() - startPos;
            QRectF newRect = rect().adjusted(0, 0, diff.x(), diff.y());

            QRectF sceneRect = scene()->sceneRect();
            if (newRect.right() > sceneRect.right())
                newRect.setWidth(sceneRect.right() - newRect.left());
            if (newRect.bottom() > sceneRect.bottom())
                newRect.setHeight(sceneRect.bottom() - newRect.top());

            setRect(newRect);
            startPos = event->pos();
        } else if (moving) {
            QPointF diff = event->pos() - startPos;
            QRectF newRect = rect();
            newRect.translate(diff.x(), diff.y());

            QRectF sceneRect = scene()->sceneRect();
            if (newRect.left() < sceneRect.left())
                newRect.moveLeft(sceneRect.left());
            if (newRect.top() < sceneRect.top())
                newRect.moveTop(sceneRect.top());
            if (newRect.right() > sceneRect.right())
                newRect.moveRight(sceneRect.right());
            if (newRect.bottom() > sceneRect.bottom())
                newRect.moveBottom(sceneRect.bottom());

            setRect(newRect);
            startPos = event->pos();
        }
        // QGraphicsRectItem::mouseMoveEvent(event);
    }

    void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) override {
        resizing = false;
        moving = false;
        // QGraphicsRectItem::mouseReleaseEvent(event);
    }

private:
    bool isResizingCorner(const QPointF& pos) const {
        const qreal cornerSize = 10;
        QRectF rect = this->rect();
        QRectF cornerRect(rect.bottomRight() - QPointF(cornerSize, cornerSize), rect.bottomRight());
        return cornerRect.contains(pos);
    }

    bool resizing;
    bool moving;
    QPointF startPos;
    QSizeF frameSize;
};
