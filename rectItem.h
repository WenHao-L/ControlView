#include <QObject>
#include <QGraphicsView>
#include <QGraphicsScene>
#include <QGraphicsRectItem>
#include <QWidget>
#include <QMouseEvent>
#include <QGraphicsSceneMouseEvent>
#include <QPen>
#include <QColor>
#include <QDebug>

class RectItem : public QObject, public QGraphicsRectItem {
    Q_OBJECT

public:
    explicit RectItem(QGraphicsItem* parent = nullptr)
        : QGraphicsRectItem(parent), resizing(false), moving(false) {
        setFlags(QGraphicsItem::ItemIsSelectable | QGraphicsItem::ItemIsMovable);
    }

    void initRect(float left, float top, float right, float bottom, int width, int height)
    {
        int x = int(left * width);
        int y = int(right * width);
        int rectWidth = int((right - left) *  width);
        int rectHeight = int((bottom - top) *  height);
        qDebug() << "初始化" << x << y << rectWidth << rectHeight;
        setRect(x, y, rectWidth, rectHeight);
        m_width = width;
        m_height = height;
    }

    void setColor(const QColor& color) {
        setPen(QPen(color));
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

        // QRectF rect = this->rect();
        // qDebug() << rect.x();
        // qDebug() << rect.y();
        // qDebug() << rect.width();
        // qDebug() << rect.height();
        // qDebug() << m_width;
        // qDebug() << m_height;
        // float leftRatio = static_cast<float>(rect.x()) / m_width;
        // float topRatio = static_cast<float>(rect.y()) / m_height;
        // float rightRatio = static_cast<float>((rect.x() + rect.width())) / m_width;
        // float bottomRatio = static_cast<float>((rect.y() + rect.height())) / m_height;
        // emit rectChanged(leftRatio, topRatio, rightRatio, bottomRatio);
    }

    void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) override {
        resizing = false;
        moving = false;

        QRectF rect = this->rect();
        qDebug() << "移动后" << rect.x() << rect.y() << rect.width() << rect.height();
        qDebug() << m_width << m_height;
        float leftRatio = static_cast<float>(rect.x()) / m_width;
        float topRatio = static_cast<float>(rect.y()) / m_height;
        float rightRatio = static_cast<float>((rect.x() + rect.width())) / m_width;
        float bottomRatio = static_cast<float>((rect.y() + rect.height())) / m_height;
        emit rectChanged(leftRatio, topRatio, rightRatio, bottomRatio);
        // QGraphicsRectItem::mouseReleaseEvent(event);
    }

signals:
    void rectChanged(float leftRatio, float topRatio, float rightRatio, float bottomRatio);

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
    int m_width;
    int m_height;
};
