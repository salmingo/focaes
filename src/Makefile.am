bin_PROGRAMS=focaes
focaes_SOURCES=ioservice_keep.cpp msgque_base.cpp tcp_asio.cpp mountproto.cpp termscreen.cpp \
               GLog.cpp \
               CameraBase.cpp \
               apgSampleCmn.cpp CameraApogee.cpp \
               udp_asio.cpp CameraGY.cpp \
               focaes.cpp

focaes_LDFLAGS=-L/usr/local/lib
AM_CPPFLAGS=-I/usr/local/include/libapogee-3.0
COMMON_LIBS=-lpthread -lcurl -lm -lrt -ltiff -lcfitsio -lxpa
BOOST_LIBS=-lboost_system-mt -lboost_thread-mt
APOGEE_LIBS=-lapogee
focaes_LDADD=${COMMON_LIBS} ${BOOST_LIBS} ${APOGEE_LIBS}
