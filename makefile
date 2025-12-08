# 컴파일러 및 플래그 설정
CC = gcc
CFLAGS = -Wall -Wextra -g -Isrc
LDFLAGS_NCURSES = -lncursesw
LDFLAGS_PTHREAD = -lpthread

# 소스 파일 및 오브젝트 파일 정의
# Common modules
GAME_LOGIC_OBJS = src/game_logic.o src/item.o
VIEW_OBJS = src/view.o

# Target specific objects
MENU_SRCS = menu.c src/score.c
MENU_OBJS = $(MENU_SRCS:.c=.o)

SINGLE_PLAY_OBJS = src/single_play.o

SERVER_OBJS = src/server.o

CLIENT_OBJS = src/client.o

# 타겟 실행 파일
TARGETS = menu single_play server client

# All object files for cleaning
ALL_OBJS = $(MENU_OBJS) $(SINGLE_PLAY_OBJS) $(SERVER_OBJS) $(CLIENT_OBJS) $(GAME_LOGIC_OBJS) $(VIEW_OBJS)

# 기본 규칙: 모든 타겟 빌드
all: $(TARGETS)

# menu 빌드 규칙
menu: $(MENU_OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS_NCURSES)

# single_play 빌드 규칙
single_play: $(SINGLE_PLAY_OBJS) $(GAME_LOGIC_OBJS) $(VIEW_OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS_NCURSES)

# server 빌드 규칙
server: $(SERVER_OBJS) $(GAME_LOGIC_OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS_PTHREAD)

# client 빌드 규칙
client: $(CLIENT_OBJS) $(VIEW_OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS_NCURSES) $(LDFLAGS_PTHREAD)

# 오브젝트 파일 생성 규칙
%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

# clean 규칙: 빌드된 파일 및 오브젝트 파일 삭제
clean:
	rm -f $(TARGETS) $(ALL_OBJS)

# PHONY: 실제 파일 이름이 아닌 명령을 위한 타겟
.PHONY: all clean
