#ifndef UTILS_OPTIONAL_HPP

#define UTILS_OPTIONAL_HPP

template <class T>
class Option {
 private:
    T& _value;
    bool _exists;
public:

    explicit Option(T value): _value(value), _exists(true) {}

    Option(): _exists(false) {}

    T& value_or(T alternate) {
        return _exists ? _value : alternate;
    }

    bool value_exists() {
        return _exists;
    }


};

#endif