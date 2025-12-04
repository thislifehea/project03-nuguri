#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

//일일히 수정하기 힘들어서 분기걸고 처리함
#ifdef _WIN32
#define usleep(x) Sleep((x) / 1000)
#endif

//분기문 추가
#ifdef _WIN32
 #include <conio.h>
 #include <windows.h>
#define getchar() _getch()
void disable_raw_mode() {}
void enable_raw_mode() {}
int kbhit(void) {
    return _kbhit();
}

#else
 #include <unistd.h>
 #include <termios.h>
 #include <fcntl.h>
// 터미널 설정
struct termios orig_termios;
// 터미널 Raw 모드 활성화/비활성화(기존코드 부분 사용)
void disable_raw_mode() { tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios); }
void enable_raw_mode() {
    tcgetattr(STDIN_FILENO, &orig_termios);
    atexit(disable_raw_mode);
    struct termios raw = orig_termios;
    raw.c_lflag &= ~(ECHO | ICANON);
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}
// 비동기 키보드 입력 확인(기존코드 부분 사용)
int kbhit() {
    struct termios oldt, newt;
    int ch;
    int oldf;
    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;
    newt.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);
    oldf = fcntl(STDIN_FILENO, F_GETFL, 0);
    fcntl(STDIN_FILENO, F_SETFL, oldf | O_NONBLOCK);
    ch = getchar();
    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
    fcntl(STDIN_FILENO, F_SETFL, oldf);
    if (ch != EOF) {
        ungetc(ch, stdin);
        return 1;
    }
    return 0;
}
#endif

// 맵 및 게임 요소 정의 (수정된 부분)
#define MAX_STAGES 2
#define MAX_ENEMIES 15 // 최대 적 개수 증가
#define MAX_COINS 30   // 최대 코인 개수 증가
#define SOUND_COIN 1 //코인 획득
#define SOUND_HIT 2 //충돌
#define SOUND_CLEAR 3 //스테이지 클리어
#define SOUND_JUMP 4 //점프
#define SOUND_DEAD 5 //게임오버

// 구조체 정의
typedef struct {
    int x, y;
    int dir; // 1: right, -1: left
} Enemy;

typedef struct {
    int x, y;
    int collected;
} Coin;

// 전역 변수
int player_x, player_y;
int stage = 0;
int score = 0;
int life = 3;   // 생명 개수
char ***map = NULL;           
int *stage_widths = NULL;     
int *stage_heights = NULL;    
int stage_count = 0;      

//게임 종료 상태
#define END_NONE 0
#define END_QUIT 1
#define END_DEAD 2

int end_type = END_NONE;

// 플레이어 상태
int is_jumping = 0;
int velocity_y = 0;
int on_ladder = 0;

// 게임 객체
Enemy enemies[MAX_ENEMIES];
int enemy_count = 0;
Coin coins[MAX_COINS];
int coin_count = 0;

void play_sound(int type) {
#ifdef _WIN32 //windows의 beep함수 사용
//주파수, 지속시간 조정
    if (type == SOUND_COIN) {
        Beep(800, 40);
    } else if(type == SOUND_HIT) {
        Beep(170, 130);
    } else if(type == SOUND_CLEAR) {
        Beep(900, 50);
        Beep(1200, 60);
    } else if(type == SOUND_JUMP) {
        Beep(500, 30);
    } else if(type == SOUND_DEAD) {
        Beep(180, 600);
    }
#else //posix환경에서는 시스템 벨 문자 \a 사용 
 char cmd[256];

 switch (type) {
        case SOUND_COIN:  
            sprintf(cmd, "powershell.exe -Command \"[console]::beep(800,40)\" > /dev/null 2>&1 &"); 
            break;
        case SOUND_JUMP:  
            sprintf(cmd, "powershell.exe -Command \"[console]::beep(600,30)\" > /dev/null 2>&1 &"); 
            break;
        case SOUND_HIT:   
            sprintf(cmd, "powershell.exe -Command \"[console]::beep(200,200)\" > /dev/null 2>&1 &"); 
            break;
        case SOUND_CLEAR: 
            // 클리어 소리는 길이가 길어 렉이 더 잘 느껴지므로, 특히 비동기화가 중요합니다.
            sprintf(cmd, "powershell.exe -Command \"[console]::beep(1200,100); Start-Sleep -m 50; [console]::beep(1500,150)\" > /dev/null 2>&1 &"); 
            break;
        case SOUND_DEAD:  
            sprintf(cmd, "powershell.exe -Command \"[console]::beep(150,800)\" > /dev/null 2>&1 &"); 
            break;
        default: return;
    }

    system(cmd);
#endif

}

// 함수 선언
void disable_raw_mode();
void enable_raw_mode();
void load_maps();
void init_stage();
void draw_game();
void update_game(char input, int* game_over);
void move_player(char input);
void move_enemies();
void check_collisions(int* game_over);
int kbhit();
void title_screen();
void ending_screen();
void dead_ending_screen();

int main() {
#ifdef _WIN32
    system("chcp 65001 > nul");
#endif
    srand(time(NULL));
    enable_raw_mode();
    load_maps();

    title_screen();  // 타이틀 화면 표시

    init_stage();

    char c = '\0';
    int game_over = 0;

    while (!game_over && stage < stage_count) {
        if (kbhit()) {
            c = getchar();
            if (c == 'q') {
                game_over = 1;
                end_type = END_QUIT;
                continue;
            }
            if (c == '\x1b') {
                getchar(); // '['
                switch (getchar()) {
                    case 'A': c = 'w'; break; // Up
                    case 'B': c = 's'; break; // Down
                    case 'C': c = 'd'; break; // Right
                    case 'D': c = 'a'; break; // Left
                }
            }
        } else {
            c = '\0';
        }

        update_game(c, &game_over);
        draw_game();
        //윈도우는 밀리초를 사용하기에 1/1000으로 만들어놓음
        #ifdef _WIN32
            Sleep(90);
        #else
        usleep(90000);
        #endif

        if (map[stage][player_y][player_x] == 'E') {
            stage++;
            score += 100;
            if (stage < stage_count) {
                play_sound(SOUND_CLEAR);
                init_stage();
            } else {
                play_sound(SOUND_CLEAR);
                game_over = 1;
            }
        }
    }

    printf("\x1b[2J\x1b[H");  // 화면 클리어

    if (stage >= MAX_STAGES) {
        ending_screen();      // 모든 스테이지 클리어 엔딩
    } else if (end_type == END_DEAD) {
    // 2) 목숨이 다 떨어져서 Game Over
    dead_ending_screen();    
    } else {
    // 3) q로 나간 경우
    printf("게임을 종료했습니다.\n");
    printf("최종 점수: %d\n", score);
    printf("아무 키를 누르면 종료합니다. . .\n");
    while (!kbhit()) {
        usleep(100000);
    }
    getchar();
}

    disable_raw_mode();
    return 0;
}

// 맵 파일 로드
void load_maps() {
    FILE *fp = fopen("map.txt", "r");
    if (!fp) {
        perror("map.txt 파일을 열 수 없습니다.");
        exit(1);
    }

    map = malloc(sizeof(char**) * 50);
    stage_widths = malloc(sizeof(int) * 50);
    stage_heights = malloc(sizeof(int) * 50);
    char line[512];
    char **cur_stage = NULL;
    int cur_h = 0;
    int cur_cap = 0;
    int width = 0;

    while (fgets(line, sizeof(line), fp)) {
        line[strcspn(line, "\r\n")] = 0;

        // 빈 줄 → 스테이지 종료
        if (strlen(line) == 0) {
            if (cur_h > 0) {
                map[stage_count] = cur_stage;
                stage_widths[stage_count] = width;
                stage_heights[stage_count] = cur_h;
                stage_count++;

                // 초기화
                cur_stage = NULL;
                cur_h = 0;
                cur_cap = 0;
                width = 0;
            }
            continue;
        }

        if (width == 0) width = strlen(line);

        if (cur_h >= cur_cap) {
            cur_cap = cur_cap == 0 ? 20 : cur_cap * 2;
            cur_stage = realloc(cur_stage, sizeof(char*) * cur_cap);
        }

        cur_stage[cur_h] = malloc(width + 1);
            
        // line 길이(short_line_len) 구함
        int short_len = strlen(line);
            
        // 짧으면 패딩으로 채움
        for (int i = 0; i < width; i++) {
            if (i < short_len) cur_stage[cur_h][i] = line[i];
            else cur_stage[cur_h][i] = ' ';
        }
        
        cur_stage[cur_h][width] = '\0';
        cur_h++;
    }

    // 마지막 스테이지 저장
    if (cur_h > 0) {
        map[stage_count] = cur_stage;
        stage_widths[stage_count] = width;
        stage_heights[stage_count] = cur_h;
        stage_count++;
    }

    fclose(fp);
}    

// 현재 스테이지 초기화
void init_stage() {
    enemy_count = 0;
    coin_count = 0;
    is_jumping = 0;
    velocity_y = 0;

    // 기존: MAP_WIDTH / MAP_HEIGHT 사용
    // 변경: 스테이지별 동적 크기 사용
    int H = stage_heights[stage];
    int W = stage_widths[stage];

    for (int y = 0; y < H; y++) {         
        for (int x = 0; x < W; x++) {     
            char cell = map[stage][y][x];

            if (cell == 'S') {
                player_x = x;
                player_y = y;
            }
            else if (cell == 'X' && enemy_count < MAX_ENEMIES) {
                enemies[enemy_count] =
                    (Enemy){x, y, (rand() % 2) ? 1 : -1};
                enemy_count++;                    
            }
            else if (cell == 'C' && coin_count < MAX_COINS) {
                coins[coin_count++] = (Coin){x, y, 0};
            }
        }
    }
}

// 게임 화면 그리기
void draw_game() {
    //윈도우용 cls 추가
    #ifdef _WIN32
    system("cls");
    printf("Stage: %d | Score: %d | Life: %d\n", stage + 1, score, life);  // 생명 출력
    printf("조작: A(왼쪽) D(오른쪽) (이동), W(위) S(아래) (사다리), Space (점프), q (종료)\n");//윈도우와 맥,리눅스 입력이 다르므로 분기작성
    #else
    printf("\x1b[2J\x1b[H");
    printf("Stage: %d | Score: %d | Life: %d\n", stage + 1, score, life);  // 생명 출력
    printf("조작: ← → (이동), ↑ ↓ (사다리), Space (점프), q (종료)\n");
    #endif

    int H = stage_heights[stage];
    int W = stage_widths[stage];    

    char **display_map = malloc(sizeof(char*) * H);

    for (int y = 0; y < H; y++) {
        display_map[y] = malloc(W + 1);

        for (int x = 0; x < W; x++) {
            char cell = map[stage][y][x];

            if (cell == 'S' || cell == 'X' || cell == 'C')
                display_map[y][x] = ' ';
            else
                display_map[y][x] = cell;
        }
        display_map[y][W] = '\0';
    }
    
    for (int i = 0; i < coin_count; i++) {
        if (!coins[i].collected) {
            display_map[coins[i].y][coins[i].x] = 'C';
        }
    }

    for (int i = 0; i < enemy_count; i++) {
        display_map[enemies[i].y][enemies[i].x] = 'X';
    }

    display_map[player_y][player_x] = 'P';

    for (int y = 0; y < H; y++) {
        printf("%s\n", display_map[y]);
        free(display_map[y]);
    }
    free(display_map);
}

// 게임 상태 업데이트
void update_game(char input, int* game_over) {
    move_player(input);
    move_enemies();
    check_collisions(game_over);   // game_over 전달
}

// 플레이어 이동 로직
void move_player(char input) {
    int H = stage_heights[stage];
    int W = stage_widths[stage];

    int next_x = player_x;
    int next_y = player_y;

    char floor_tile = (player_y + 1 < H)
        ? map[stage][player_y + 1][player_x]
        : '#';
    char current_tile = map[stage][player_y][player_x];

    on_ladder = (current_tile == 'H');

    switch (input) {
        case 'a': next_x--; break;
        case 'd': next_x++; break;
        case 'w': if (on_ladder) next_y--; break;
        case 's': if (on_ladder && player_y + 1 < H &&
                      map[stage][player_y + 1][player_x] != '#')
                      next_y++;
            break;
        case ' ':
            if (!is_jumping && (floor_tile == '#' || on_ladder)) {
                is_jumping = 1;
                velocity_y = -2;
                play_sound(SOUND_JUMP);
            }
            break;
    }

    if (next_x >= 0 && next_x < W &&
        map[stage][player_y][next_x] != '#')
        player_x = next_x;
    
    if (on_ladder && (input == 'w' || input == 's')) {
        if (next_y >= 0 && next_y < H &&
            map[stage][next_y][player_x] != '#')
        {
            player_y = next_y;
            is_jumping = 0;
            velocity_y = 0;
        }
    } 
    else {
        if (is_jumping) {
            next_y = player_y + velocity_y;
            if(next_y < 0) next_y = 0;
            velocity_y++;

            if (velocity_y < 0 && next_y < H && map[stage][next_y][player_x] == '#') {
                velocity_y = 0;
            } else if (next_y < H) {
                player_y = next_y;
            }
            
            if (player_y + 1 < H &&
                map[stage][player_y + 1][player_x] == '#')
            {
                is_jumping = 0;
                velocity_y = 0;
            }
        } else {
            if (floor_tile != '#' && floor_tile != 'H') {
                 if (player_y + 1 < H) player_y++;
                 else init_stage();
            }
        }
    }
    
    if (player_y >= H) {
        play_sound(SOUND_DEAD);
        init_stage();
    }
    for (int i = 0; i < coin_count; i++) {
        if (!coins[i].collected && player_x == coins[i].x && player_y == coins[i].y) {
            coins[i].collected = 1;
            score += 20;
            play_sound(SOUND_COIN);
        }
    }
}


// 적 이동 로직
void move_enemies() {

    int H = stage_heights[stage];
    int W = stage_widths[stage];

    for (int i = 0; i < enemy_count; i++) {

        int next_x = enemies[i].x + enemies[i].dir;

        if (next_x < 0 || next_x >= W ||
            map[stage][enemies[i].y][next_x] == '#' ||
            (enemies[i].y + 1 < H &&
             map[stage][enemies[i].y + 1][next_x] == ' '))
        {
            enemies[i].dir *= -1;
        }
        else {
            enemies[i].x = next_x;
        }
    }
}

// 충돌 감지 로직
void check_collisions(int* game_over) {
    for (int i = 0; i < enemy_count; i++) {
        if (player_x == enemies[i].x && player_y == enemies[i].y) {
            life--;  // 생명 감소
            score = 0; // 점수 초기화
            if (life <= 0) {
                *game_over = 1;
                end_type = END_DEAD;
                play_sound(SOUND_DEAD);   // 게임 종료
                return;
            }
            play_sound(SOUND_HIT);
            init_stage();         // 스테이지 restart
            return;
        }
    }
}

// 타이틀 화면
void title_screen() {
    printf("\x1b[2J\x1b[H");  // 화면 클리어
    printf("========================================\n");
    printf("                TITLE SCREEN             \n");
    printf("========================================\n\n");
    printf("   조작법\n");
    printf("   ← →  : 좌우 이동\n");
    printf("   ↑ ↓  : 사다리 오르내리기\n");
    printf("   Space: 점프\n");
    printf("   q    : 게임 종료\n\n");
    printf("   코인을 모으고 E 지점까지 도달하세요!\n\n");
    printf("   아무 키나 눌러서 게임을 시작합니다...\n");

    while (!kbhit()) {
        usleep(100000);
    }
    getchar();

    while (kbhit()) {
        getchar();
    }
}

// 엔딩 화면
void ending_screen() {
    printf("\x1b[2J\x1b[H");  // 화면 클리어
    printf("========================================\n");
    printf("           STAGE ALL CLEAR!!            \n");
    printf("========================================\n\n");
    printf("   축하합니다! 모든 스테이지를 클리어했습니다!\n");
    printf("   최종 점수: %d\n\n", score);
    printf("   아무 키나 눌러서 게임을 종료합니다...\n");

    while (kbhit()) {
        getchar();
    }

    while (!kbhit()) {
        usleep(100000);
    }
    getchar();
}

void dead_ending_screen() {
    printf("\x1b[2J\x1b[H");  // 화면 클리어
    printf("========================================\n");
    printf("               GAME  OVER               \n");
    printf("========================================\n\n");
    printf("   모든 목숨을 잃었습니다...\n");
    printf("   아무 키나 눌러서 게임을 종료합니다...\n");

    while (!kbhit()) {
        usleep(100000);
    }
    getchar();
}