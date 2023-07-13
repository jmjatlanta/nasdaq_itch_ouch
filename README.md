## Various pieces for NASDAQ ITCH, OUCH, and SoupBinTCP



## Simple header-only C++ objects for NASDAQ ITCH 5.0 protocol

Take a peek in the `itch.h` header file for details.

There is a constructor if you want to create a blank object, and a constructor where you pass in a char pointer
that contains the record.

There are getters and setters for int and string. Below is a brief example. See `test` directory
for more examples:

```
    itch::system_event msg;
    msg.set_int(msg.STOCK_LOCATE, 1);
    msg.set_int(msg.TRACKING_NUMBER, 2);
    msg.set_int(msg.TIMESTAMP, 6);
    msg.set_string(msg.EVENT_CODE, "O");
    int timestamp = msg.get_int(msg.TIMESTAMP); // should equal 6
```

or if you're the client taking in a feed:

```
    char message_type = network.peek_byte();
    if (message_type == 'S')
    {
        char* record = network.read_bytes(itch::SYSTEM_EVENT_LEN);
        itch::system_event msg(record);
        int timestamp = msg.get_int(msg.TIMESTAMP);
        // more code goes here
    }
```

TODO:
- Test each object for their length
- Test each object for their prefix
- Test each field of each object for setting and retrieval
