#pragma once

#include <QList>

#include "Contact.h"

// Contacts the user creates on this machine, for when there is no remote XML
// provisioning (or to complement it). Stored as JSON next to the call
// history; same fields as a remote contact so both render identically.
class LocalContactsStore {
public:
    static QList<Contact> load();
    static void save(const QList<Contact> &contacts);
    static void clear();

    static QString filePath();
};
