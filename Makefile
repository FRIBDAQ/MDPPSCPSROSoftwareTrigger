TARGET=MDPPSCPSROSoftTrigger

all: $(TARGET) 

%: %.cpp
	g++ -g -o $@ $^ \
	-I$(DAQROOT)/include -L$(DAQLIB)	\
	-ldataformat -ldaqio -lException -Wl,-rpath=$(DAQLIB) -std=c++11

clean:
	rm -f $(TARGET)