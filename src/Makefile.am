bin_PROGRAMS=focaes
focaes_SOURCES=ioservice_keep.cpp msgque_base.cpp tcp_asio.cpp mountproto.cpp termscreen.cpp \
               GLog.cpp \
               CameraBase.cpp \
               udp_asio.cpp CameraGY.cpp \
               focaes.cpp

focaes_LDFLAGS=-L/usr/local/lib
COMMON_LIBS=-lpthread -lm -lrt -lcfitsio -lxpa
BOOST_LIBS=-lboost_system-mt -lboost_thread-mt
focaes_LDADD=${COMMON_LIBS} ${BOOST_LIBS}
