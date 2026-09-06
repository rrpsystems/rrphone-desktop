#pragma once

#include <QPushButton>

// A keypad key drawn like a classic desk-phone / softphone key: a large digit
// with the small letter group inline next to it ("2ABC", "7PQRS", "0+").
// QPushButton can't mix two font sizes in its own text, so the label is
// painted by hand instead.
class DialKeyButton : public QPushButton {
    Q_OBJECT

public:
    DialKeyButton(QChar digit, const QString &letters, QWidget *parent = nullptr);

    QChar digit() const { return m_digit; }

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QChar m_digit;
    QString m_letters;
};
