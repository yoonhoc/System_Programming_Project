# 컴파일러 및 플래그 설정
CC = gcc
CFLAGS = -Wall -Wextra -g -Isrc
LDFLAGS_NCURSES = -lncursesw
LDFLAGS_PTHREAD = -lpthread

# 소스 파일 및 오브젝트 파일 정의
MENU_SRCS = menu.c src/score.c
MENU_OBJS = $(MENU_SRCS:.c=.o)

SINGLE_PLAY_SRCS = src/single_play.c src/item.c
SINGLE_PLAY_OBJS = $(SINGLE_PLAY_SRCS:.c=.o)

SERVER_SRCS = src/server.c src/item.c
SERVER_OBJS = $(SERVER_SRCS:.c=.o)

CLIENT_SRCS = src/client.c
CLIENT_OBJS = $(CLIENT_SRCS:.c=.o)

# 타겟 실행 파일
TARGETS = menu single_play server client

# 기본 규칙: 모든 타겟 빌드
all: $(TARGETS)

# menu 빌드 규칙
menu: $(MENU_OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS_NCURSES)

# single_play 빌드 규칙
single_play: $(SINGLE_PLAY_OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS_NCURSES)

# server 빌드 규칙
server: $(SERVER_OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS_PTHREAD)

# client 빌드 규칙
client: $(CLIENT_OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS_NCURSES) $(LDFLAGS_PTHREAD)

# 오브젝트 파일 생성 규칙
%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

# clean 규칙: 빌드된 파일 및 오브젝트 파일 삭제
clean:
	rm -f $(TARGETS) $(MENU_OBJS) $(SINGLE_PLAY_OBJS) $(SERVER_OBJS) $(CLIENT_OBJS)

# PHONY: 실제 파일 이름이 아닌 명령을 위한 타겟
.PHONY: all clean
