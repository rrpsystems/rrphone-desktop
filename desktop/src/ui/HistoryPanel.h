#pragma once

#include <QWidget>

#include "history/CallHistoryStore.h"

class QListWidget;

// D-08 — call history page. Double-clicking an entry redials it.
class HistoryPanel : public QWidget {
    Q_OBJECT

public:
    explicit HistoryPanel(QWidget *parent = nullptr);

public slots:
    // Re-reads from disk. Called whenever the page is shown, so it never
    // shows a stale list after a call.
    void reload();

signals:
    void callRequested(const QString &number);

private:
    QListWidget *m_listWidget;
};
