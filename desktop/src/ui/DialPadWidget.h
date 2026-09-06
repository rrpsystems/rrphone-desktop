#pragma once

#include <QWidget>

// D-02 / D-05 — the 3x4 keypad grid. It only reports which key was pressed;
// MainWindow decides whether that means "append to the number being dialed"
// or "send a DTMF tone into the active call".
class DialPadWidget : public QWidget {
    Q_OBJECT

public:
    explicit DialPadWidget(QWidget *parent = nullptr);

signals:
    void digitPressed(QChar digit);
};
