#ifndef QQMLHELPERS_H
#define QQMLHELPERS_H

#include <QObject>

#define QML_WRITABLE_PROPERTY(type, name) \
    protected: \
        Q_PROPERTY (type name READ get_##name WRITE set_##name NOTIFY name##Changed) \
    private: \
        type m_##name; \
    public: \
        type get_##name () const { \
            return m_##name ; \
        } \
    public Q_SLOTS: \
        bool set_##name (type name) { \
            bool ret = false; \
            if ((ret = m_##name != name)) { \
                m_##name = name; \
                emit name##Changed (m_##name); \
            } \
            return ret; \
        } \
    Q_SIGNALS: \
        void name##Changed (type name); \
    private:

#define QML_READONLY_PROPERTY(type, name) \
    protected: \
        Q_PROPERTY (type name READ get_##name NOTIFY name##Changed) \
    private: \
        type m_##name; \
    public: \
        type get_##name () const { \
            return m_##name ; \
        } \
        bool update_##name (type name) { \
            bool ret = false; \
            if ((ret = m_##name != name)) { \
                m_##name = name; \
                emit name##Changed (m_##name); \
            } \
            return ret; \
        } \
    Q_SIGNALS: \
        void name##Changed (type name); \
    private:

#define QML_CONSTANT_PROPERTY(type, name) \
    protected: \
        Q_PROPERTY (type name READ get_##name CONSTANT) \
    private: \
        type m_##name; \
    public: \
        type get_##name () const { \
            return m_##name ; \
        } \
    private:

#define QML_ENUM_CLASS(name, ...) \
    class name : public QObject { \
        Q_OBJECT \
    public: \
        enum Type { __VA_ARGS__ }; \
        Q_ENUMS (Type) \
    };

/* Equality guard for the model-backed setters below.
 *
 * Skipping a write whose value is unchanged removes the redundant
 * name##Changed emission that used to fire on every server poll. Two
 * conditions must hold for that to be safe, and both are encoded here:
 *
 *  1. The FIRST write must always go through, even when the incoming value
 *     equals the default-constructed member. setData() is what materialises
 *     the role on the QStandardItem; a role that was never written reads back
 *     as an invalid QVariant, i.e. "undefined" in a QML delegate rather than
 *     "" / false / 0. Items in this codebase are routinely populated with
 *     default values before appendRow() (RoomItem's constructor,
 *     UpdatePackagesModel::load, ControlPanelModel::load, ...), so a guard
 *     that compared values alone would silently drop those roles. The
 *     m_##name##Primed flag makes the first write unconditional.
 *
 *  2. Comparison must never report "equal" for values that actually differ.
 *     operator== is used verbatim: deep compare for QString/QStringList,
 *     integral compare for enums/int/bool, exact IEEE compare for double.
 *     Exact compare is deliberate for floating point - an epsilon/fuzzy
 *     compare would swallow small but genuine changes (audio playback
 *     position, temperatures), and NaN != NaN merely re-emits, which is the
 *     safe direction to err in.
 *
 * Note that QStandardItem::setData() already drops identical values without
 * emitting dataChanged(), so this guard adds nothing on the model side; what
 * it removes is the redundant Qt signal driving QML property bindings.
 */
#define QML_PROPERTY_MODEL_GUARD(name) \
    if (m_##name##Primed && m_##name == name) \
        return; \
    m_##name##Primed = true;

#define QML_WRITABLE_PROPERTY_MODEL(type, name, role) \
    protected: \
        Q_PROPERTY (type name READ get_##name WRITE set_##name NOTIFY name##Changed) \
    private: \
        type m_##name {}; \
        bool m_##name##Primed = false; \
    public: \
        type get_##name () const { \
            return m_##name ; \
        } \
    public Q_SLOTS: \
        void set_##name (type name) { \
            QML_PROPERTY_MODEL_GUARD (name) \
            m_##name = name; \
            setData(name, role); \
            emit name##Changed (m_##name); \
        } \
    Q_SIGNALS: \
        void name##Changed (type name); \
    private:

#define QML_READONLY_PROPERTY_MODEL(type, name, role) \
    protected: \
        Q_PROPERTY (type name READ get_##name NOTIFY name##Changed) \
    private: \
        type m_##name {}; \
        bool m_##name##Primed = false; \
    public: \
        type get_##name () const { \
            return m_##name ; \
        } \
        void update_##name (type name) { \
            QML_PROPERTY_MODEL_GUARD (name) \
            m_##name = name; \
            setData(name, role); \
            emit name##Changed (m_##name); \
        } \
    Q_SIGNALS: \
        void name##Changed (type name); \
    private:


class QmlProperty : public QObject { Q_OBJECT }; // FIXME : to avoid "no suitable class found" MOC note

#endif // QQMLHELPERS_H
