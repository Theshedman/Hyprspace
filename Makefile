CXXFLAGS += -shared -fPIC --no-gnu-unique -Wall -g -DWLR_USE_UNSTABLE -std=c++23 -O2
INCLUDES = `pkg-config --cflags pixman-1 libdrm hyprland hyprutils hyprlang pangocairo libinput libudev wayland-server xkbcommon`

LIBS = `pkg-config --libs pixman-1 libdrm hyprland hyprutils hyprlang pangocairo libinput libudev wayland-server xkbcommon`
SRC = $(wildcard src/*.cpp)
TARGET = Hyprspace.so
PREFIX ?= /usr/local
LIBDIR ?= $(PREFIX)/lib

all:
	$(CXX) $(CXXFLAGS) $(INCLUDES) $(SRC) $(LIBS) -o $(TARGET)

install:
	install -D -m 0755 $(TARGET) $(DESTDIR)$(LIBDIR)/$(TARGET)

clean:
	rm -f ./$(TARGET)

withhyprpmheaders: export PKG_CONFIG_PATH = $(XDG_DATA_HOME)/hyprpm/headersRoot/share/pkgconfig
withhyprpmheaders: all
