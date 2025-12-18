# # 컴파일러 및 플래그 설정
# CC = gcc
# CFLAGS = -Wall -Wextra -g -Isrc
# LDFLAGS_NCURSES = -lncursesw
# LDFLAGS_PTHREAD = -lpthread

# # 소스 파일 및 오브젝트 파일 정의
# # Common modules
# GAME_LOGIC_OBJS = src/game_logic.o src/item.o
# VIEW_OBJS = src/view.o

# # Target specific objects
# MENU_SRCS = menu.c src/score.c
# MENU_OBJS = $(MENU_SRCS:.c=.o)

# SINGLE_PLAY_OBJS = src/single_play.o

# SERVER_OBJS = src/server.o

# CLIENT_OBJS = src/client.o

# # 타겟 실행 파일
# TARGETS = menu single_play server client

# # All object files for cleaning
# ALL_OBJS = $(MENU_OBJS) $(SINGLE_PLAY_OBJS) $(SERVER_OBJS) $(CLIENT_OBJS) $(GAME_LOGIC_OBJS) $(VIEW_OBJS)

# # 기본 규칙: 모든 타겟 빌드
# all: $(TARGETS)

# # menu 빌드 규칙
# menu: $(MENU_OBJS)
# 	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS_NCURSES)

# # single_play 빌드 규칙
# single_play: $(SINGLE_PLAY_OBJS) $(GAME_LOGIC_OBJS) $(VIEW_OBJS)
# 	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS_NCURSES)

# # server 빌드 규칙
# server: $(SERVER_OBJS) $(GAME_LOGIC_OBJS)
# 	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS_PTHREAD)

# # client 빌드 규칙
# client: $(CLIENT_OBJS) $(VIEW_OBJS)
# 	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS_NCURSES) $(LDFLAGS_PTHREAD)

# # 오브젝트 파일 생성 규칙
# %.o: %.c
# 	$(CC) $(CFLAGS) -c -o $@ $<

# # clean 규칙: 빌드된 파일 및 오브젝트 파일 삭제
# clean:
# 	rm -f $(TARGETS) $(ALL_OBJS)

# # PHONY: 실제 파일 이름이 아닌 명령을 위한 타겟
# .PHONY: all clean
# 컴파일러 및 플래그 설정


# 컴파일러 및 플래그 설정
CC = gcc
CFLAGS = -Wall -Wextra -g -I./include
LDFLAGS_NCURSES = -lncursesw
LDFLAGS_PTHREAD = -lpthread

# 디렉토리 설정
SRCDIR = src
INCDIR = include
OBJDIR = obj
BINDIR = bin
DATADIR = data

# 소스 파일 정의
# Common modules
GAME_LOGIC_SRCS = $(SRCDIR)/game_logic.c $(SRCDIR)/item.c
VIEW_SRCS = $(SRCDIR)/view.c
COMMON_SRCS = $(SRCDIR)/common.c $(SRCDIR)/score.c

# Target specific sources
MENU_SRCS = $(SRCDIR)/menu.c $(SRCDIR)/score.c
SINGLE_PLAY_SRCS = $(SRCDIR)/single_play.c
SERVER_SRCS = $(SRCDIR)/server.c
CLIENT_SRCS = $(SRCDIR)/client.c

# 오브젝트 파일 정의
GAME_LOGIC_OBJS = $(patsubst $(SRCDIR)/%.c,$(OBJDIR)/%.o,$(GAME_LOGIC_SRCS))
VIEW_OBJS = $(patsubst $(SRCDIR)/%.c,$(OBJDIR)/%.o,$(VIEW_SRCS))
COMMON_OBJS = $(patsubst $(SRCDIR)/%.c,$(OBJDIR)/%.o,$(COMMON_SRCS))

MENU_OBJS = $(patsubst $(SRCDIR)/%.c,$(OBJDIR)/%.o,$(MENU_SRCS))
SINGLE_PLAY_OBJS = $(patsubst $(SRCDIR)/%.c,$(OBJDIR)/%.o,$(SINGLE_PLAY_SRCS))
SERVER_OBJS = $(patsubst $(SRCDIR)/%.c,$(OBJDIR)/%.o,$(SERVER_SRCS))
CLIENT_OBJS = $(patsubst $(SRCDIR)/%.c,$(OBJDIR)/%.o,$(CLIENT_SRCS))

# 타겟 실행 파일
MENU = $(BINDIR)/menu
SINGLE = $(BINDIR)/single_play
SERVER = $(BINDIR)/server
CLIENT = $(BINDIR)/client

TARGETS = $(MENU) $(SINGLE) $(SERVER) $(CLIENT)

# All object files for cleaning
ALL_OBJS = $(MENU_OBJS) $(SINGLE_PLAY_OBJS) $(SERVER_OBJS) $(CLIENT_OBJS) \
           $(GAME_LOGIC_OBJS) $(VIEW_OBJS) $(COMMON_OBJS)

# 기본 규칙: 모든 타겟 빌드
all: dirs $(TARGETS)

# 필요한 디렉토리 생성
dirs:
	@mkdir -p $(OBJDIR) $(BINDIR) $(DATADIR)

# menu 빌드 규칙 (메인 프로그램)
$(MENU): $(MENU_OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS_NCURSES)

# single_play 빌드 규칙
$(SINGLE): $(SINGLE_PLAY_OBJS) $(GAME_LOGIC_OBJS) $(VIEW_OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS_NCURSES)

# server 빌드 규칙
$(SERVER): $(SERVER_OBJS) $(GAME_LOGIC_OBJS) $(COMMON_OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS_PTHREAD)


# client 빌드 규칙
$(CLIENT): $(CLIENT_OBJS) $(VIEW_OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS_NCURSES) $(LDFLAGS_PTHREAD)

# src 폴더의 .c 파일을 obj 폴더의 .o 파일로 컴파일
$(OBJDIR)/%.o: $(SRCDIR)/%.c
	$(CC) $(CFLAGS) -c -o $@ $ $<

# clean 규칙: 빌드된 파일 및 오브젝트 파일 삭제
clean:
	rm -f $(OBJDIR)/*.o
	rm -f $(TARGETS)
	rm -f $(DATADIR)/scores.dat

# 완전 삭제 (폴더까지)
distclean: clean
	rm -rf $(OBJDIR) $(BINDIR)

# 재빌드
rebuild: clean all

# 메인 프로그램 실행
run: $(MENU)
	./$(MENU)

# PHONY: 실제 파일 이름이 아닌 명령을 위한 타겟
.PHONY: all clean distclean rebuild dirs run