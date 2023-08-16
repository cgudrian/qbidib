qt_add_library(bidib STATIC
    include/bidib/address.h address.cpp
    include/bidib/error.h
    include/bidib/bidib_messages.h
    include/bidib/message.h message.cpp
    include/bidib/node.h node.cpp
    include/bidib/serialconnection.h serialconnection.cpp
    include/bidib/serialtransport.h serialtransport.cpp
    include/bidib/pack.h

    crc.h crc.cpp
    messagenames.cpp
)
target_include_directories(bidib PRIVATE include/bidib)
target_include_directories(bidib PUBLIC ../tl include)
target_link_libraries(bidib PUBLIC Qt6::Core Qt6::CorePrivate Qt6::SerialPort)

qt_add_executable(bidib-test tst_bidib.cpp)
target_link_libraries(bidib-test PRIVATE Qt6::Test bidib)
add_test(NAME bidib-test COMMAND bidib-test)
