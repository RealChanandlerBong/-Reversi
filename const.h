#ifndef CONST_H
#define CONST_H


// enum classes

enum COLOR {
    BLACK = 1,
    WHITE = -1,
    FREE = 0,
    HINT = 2,
};

enum ROLE {
    SINGLE = 0,
    HOST = 1,
    GUEST = 2,
    UNKNOWN = -1,
};

enum DIR {
    UP = 0,
    DOWN = 1,
    LEFT = 2,
    RIGHT = 3,
    LEFT_UP = 4,
    LEFT_DOWN = 5,
    RIGHT_UP = 6,
    RIGHT_DOWN = 7,
};

// socket keywords

const char* const TYPE = "type";
const char* const CONTENT = "content";
const char* const SYN = "syn";

const char* const CONNECT = "connect";
const char* const MOVE = "move";
const char* const SKIP = "skip";
const char* const REGRET = "regret";
const char* const YIELD = "yield";
const char* const LEAVE = "leave";
const char* const NEW = "new";

const char* const REQUEST = "request";
const char* const ACCEPTED = "accepted";
const char* const REJECTED = "rejected";

const char* const SEP = "_";

#endif // CONST_H
