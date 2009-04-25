/*
 *
 *
 */

#include <lip.h>
#include <types.h>
#include <loader.h>
#include <bpb.h>

#include <shared.h>

#include "fsys.h"
#include "fsd.h"
#include "term.h"

extern lip2_t *l;
extern bios_parameters_block *bpb;
extern FileTable ft;

extern struct variable_list_struct {
  char *name;
  char *value;
} variable_list[VARIABLES_MAX];

extern struct builtin *builtins[];

extern int menu_timeout;

struct multiboot_info *m;
struct term_entry *t;

static char state = 0;

int  default_item = -1;

/* menu colors */
int background_color = 0; // black
int foreground_color = 7; // white
int background_color_hl = 0;
int foreground_color_hl = 7;


int screen_bg_color = 0;
int screen_fg_color = 7;
int screen_bg_color_hl = 0;
int screen_fg_color_hl = 7;

int num_items = 0;
int scrollnum = 0;

void show_background_screen(void);
void draw_menu(int item, int shift);

void create_lip_module(lip2_t **l);
void multi_boot(void);

char *
skip_to (int after_equal, char *cmdline);

int (*process_cfg_line)(char *line);

//#pragma aux multi_boot     "*"
#pragma aux m              "*"
#pragma aux l              "*"

#pragma aux entry_addr "*"

extern entry_func entry_addr;

int boot_func (char *arg, int flags);
int root_func (char *arg, int flags);
int kernel_func (char *arg, int flags);
int module_func (char *arg, int flags);
int modaddr_func (char *arg, int flags);
int lipmodule_func (char *arg, int flags);
int vbeset_func (char *arg, int flags);
int set_func (char *arg, int flags);
int varexpand_func (char *arg, int flags);

void panic(char *msg, char *file);
int  abbrev(char *s1, char *s2, int n);
char *strip(char *s);
char *trim(char *s);
char *wordend(char *s);

char *menu_items = (char *)MENU_BUF;
int  menu_cnt, menu_len = 0;
char *config_lines;
int  config_len = 0;

char cmdbuf[0x200];
char path[] = "freeldr";
static int promptlen;

#define SCREEN_WIDTH  80
#define SCREEN_HEIGHT 25

extern int menu_width;
extern int menu_height;

typedef struct script script_t;
// a structure corresponding to a
// boot script or menu item
typedef struct script
{
  char *scr;      // a pointer to a script itself
  int  num;       // number of commands in boot script
  char *title;    // menu title
  script_t *next; // next item
  script_t *prev; // previous item
} script_t;

script_t *menu_first, *menu_last;

int process_cfg(char *cfg);

void init(lip2_t *l)
{

}

int
process_cfg_line1(char *line)
{
  int    n;
  int    i;
  char   *s, *p;
  char   *title; // current menu item title
  static int  section = 0;
  static script_t *sc = 0, *sc_prev = 0;
  struct builtin **b;

  if (!*line) return 1;

  if (abbrev(line, "title", 5))
  {
    section++;
    num_items++;
    s =  skip_to(1, line);
    p = menu_items;

    title = p + menu_len;
    while (p[menu_len++] = *s++) ;

    // add a script_t structure after previous menu item name
    sc = (script_t *)((char *)p + menu_len);
    sc->num = 0;
    sc->title = title;
    sc->prev = sc_prev;
    if (sc_prev) sc_prev->next = sc;
    sc->next = 0;
    menu_len += sizeof(script_t);
    if (section == 1) menu_first = sc;
    sc_prev = sc;
    menu_last = sc;
  }
  else if (section)
  {
    s = line;
    p = config_lines;
    // if this is a 1st command
    if (!sc->num) sc->scr = p + config_len;
    sc->num++;
    // copy a line to buffer
    while (p[config_len++] = *s++) ;
  }
  else
  {
    for (b = builtins; *b; b++)
    {
      if (abbrev(line, (*b)->name, strlen((*b)->name)))
      {
        line = skip_to(1, line);
        if (((*b)->func)(line, 0x2))
        {
          printf("Error occured during execution of %s\r\n", (*b)->name);
          return 0;
        }
        return 1;
      } 
    }
  }

  return 1;
}


int
exec_line(char *line)
{
  int i;
  char *p, *s = line;
  struct builtin **b;

  if (!*line) return 1;

  // change '\\' into '/'
  p = line;
  while (*p++)
    if (*p == '\\') *p = '/';

  for (b = builtins; *b; b++)
  {
    if (abbrev(line, (*b)->name, strlen((*b)->name)))
    {
      line = skip_to(1, line);
      if (((*b)->func)(line, 0x2))
      {
        printf("Error occured during execution of %s\r\n", (*b)->name);
        return 0;
      }
      break;
    }
  }

  return 1;
}

// get next menu item from user input
int
get_user_input(int *item, int *shift)
{
  int c;

  // scan code
  for (;;)
  {
    c = t->getkey();
    //printf("0x%x ", c);
    switch (c)
    {
      case 0xe:   // down arrow
      {
        ++*item;
        if (*item == num_items + 1) *item = 0;
        return 1;
      }
      case 0x10:  // up arrow
      {
        --*item;
        if (*item == -1) *item = num_items;
        return 1;
      }
      case 0x6:   // right arrow
      {
        if (*shift >= -menu_width) --*shift;
        return 1;
      }
      case 0x2:   // left arrow
      {
        if (*shift < 0) ++*shift;
        return 1;
      }
      case 0x3: // pgdn
      {
        *item += menu_height - 1;
        if (*item >= num_items + 1) *item = 0;
        return 1;
      }
      case 0x7: // pgup
      {
        *item -= menu_height - 1;
        if (*item < 0) *item = num_items;
        return 1;
      }
      case 0x7400: // ctrl-right
      case 0x5:    // end
      {
        *shift += menu_width;
        return 1;
      }
      case 0x7300: // ctrl-left
      case 0x1:    // home
      {
        *shift -= menu_width;
        return 1;
      }
      case 0x3e00: // F4
      case 0x1245: // E
      case 0x1265: // e
      {
        if (state == 0)
        {
          t->cls();
          state = 2;
          //for (ii = 0; ii < 0x1000; ii++) ;
        }
        return 0;
      }
      case 0x11b:  // esc
      {
        int ii;
        if (state == 0) 
        {
          state++;
          t->cls();
          //for (ii = 0; ii < 0x1000; ii++) ;
        }
        return 0;
      }
      case 0x1c0d: // enter
      {
        ++*item;
        return 0;
      }
      default:
       {
         printf("0x%x\r\n", c);
         return 1;
       }
    }
  }

  *shift = 0;

  return 1;
}

void show_background_screen(void)
{
  int i;
  char *s1 = "--== FreeLdr ver. 0.0.2. ==--";
  char *s2 = "(c) osFree project, 2008 Jun 23. licensed under GNU GPL v.2";
  int  l, n;

  t->setcolor((char)screen_fg_color    | ((char)screen_bg_color << 4),
              (char)screen_fg_color_hl | ((char)screen_bg_color_hl << 4));
  t->cls();

  t->setcolorstate(COLOR_STATE_NORMAL);

  /* draw the frame */

  t->gotoxy(1, 0);
  t->putchar(0xda);
  for (i = 0; i < 80 - 4; i++) t->putchar(0xc4);
  t->putchar(0xbf);


  for (i = 0; i < 25 - 2; i++)
  {
    t->gotoxy(1,  1 + i); t->putchar(0xb3);
    t->gotoxy(78, 1 + i); t->putchar(0xb3);
  }

  t->gotoxy(1, 24);
  t->putchar(0xc0);

  for (i = 0; i < 80 - 4; i++) t->putchar(0xc4);

  t->gotoxy(78, 24);
  t->putchar(0xd9);

  t->setcolor(0x75, 7);

  /* header line */
  t->gotoxy(4, 0);
  l = grub_strlen(s1);
  n = (80 - 8 - l) / 2;
  for (i = 0; i < n; i++) t->putchar(' ');
  for (i = 0; i < l; i++) t->putchar(s1[i]);
  for (i = n + l; i < 80 - 8; i++) t->putchar(' ');

  /* footer line */
  t->gotoxy(4, 24);
  for(i = 0; i < 80 - 8; i++) t->putchar(' ');

  /* copyright */
  t->gotoxy(5, 24);
  printf("%s", s2);

  t->setcolor((char)foreground_color    | ((char)background_color << 4),
              (char)foreground_color_hl | ((char)background_color_hl << 4));
}

void
invert_colors(int *col1, int *col2)
{
  int col;

  col = *col1;
  *col1 = *col2;
  *col2 = col;

  t->setcolor((char)foreground_color | ((char)background_color << 4),
              (char)foreground_color_hl | ((char)background_color_hl << 4));
}

// draw a menu with selected item shifted
// by 'shift' symbols to right/left
void draw_menu(int item, int shift)
{
  int i = 0, j, l, k, m;
  script_t *sc;
  char s[4];
  char str[4];
  char spc[0x80];
  char buf[0x100];
  char *p;

  //t->setcolorstate(COLOR_STATE_NORMAL);

  // background
  //t->setcolor(0 | (3 << 4), 7 | (3 << 4));
  // clear screen
  //t->cls();
  // 5 - normal (pink), 3 - highlighted (magenta)
  t->setcolor((char)foreground_color    | ((char)background_color << 4),
              (char)foreground_color_hl | ((char)background_color_hl << 4));

  t->gotoxy(12, 5);
  l = 0;
  buf[l++] = 0xda;
  while (l < menu_width) buf[l++] = 0xc4;
  buf[l++] = 0xbf; buf[l] = '\0';
  printf("%s", buf);

  grub_memset(buf, 0, sizeof(buf));

  if (item + 2 > menu_height) scrollnum = item + 1 - menu_height;
  if (item == num_items + 1)  scrollnum = 0;
  if (!item) scrollnum = 0;

  sc = menu_first;

  if (shift > 0)
  {
    for (i = 0; i < shift; i++) spc[i] = ' ';
    spc[i] = '\0';
    m = 0;
  }
  else
  {
    spc[0] = '\0';
    m = -shift;
  }

  for (k = 0; k < scrollnum; k++) sc = sc->next;

  for (i = 0; i < menu_height; i++)
  {
    j = scrollnum + i;

    t->gotoxy(12, 6 + i);

    printf("%c ", 0xb3);

    // show highlighted menu string in inverse color
    if (j == item) invert_colors(&foreground_color, &background_color);

    sprintf(s, "%d", j);
    l = grub_strlen(s);
    if (l == 1) grub_strcat(str, " ", s);
    if (l == 2) grub_strcpy(str, s);
    sprintf(buf, "%s%s. %s", spc, str, sc->title);

    p = buf + m;
    l = grub_strlen(p);

    while (l > menu_width - 3) p[l--] = '\0';
    while (l < menu_width - 3) p[l++] = ' ';
    p[l] = '\0';

    printf("%s", p);

    grub_memset(buf, 0, sizeof(buf));

    // show highlighted menu string in inverse color
    if (j == item) invert_colors(&foreground_color, &background_color);

    printf(" %c", 0xb3);

    sc = sc->next;
    //i++;
  }

  t->gotoxy(12, 6 + i);
  l = 0;
  buf[l++] = 0xc0;
  while (l < menu_width) buf[l++] = 0xc4;
  buf[l++] = 0xd9; buf[l] = '\0';
  printf("%s", buf);

  t->setcursor(0);

  t->setcolor(7, 7);
}

void showpath(void)
{
  printf("\r\n[%s] ", path);
  promptlen = strlen(path) + 3; 
  t->setcursor(5);
}

char *getcmd(int key)
{
  int ch = key; // keycode
  int ind = 0;  // cmd line length
  int cur = 0;  // cursor pos in the cmd line
  int pos;      // cursor pos on the screen
  int p;

  memset(cmdbuf, 0, sizeof(cmdbuf));
 
  do 
  {
    switch (ch)
    {
       case 0x0:    // reget char
         ch = t->getkey();
         continue;
       case 0x2:    // left arrow
         cur--;
         if (cur <= 0) cur = 0;
         pos = t->getxy();
         if ((pos >> 8) <= promptlen + 1) pos = ((promptlen + 1) << 8) | (pos & 0xff);
         t->gotoxy((pos >> 8) - 1, pos & 0xff);
         t->setcursor(5);
         ch = 0x0;
         continue;
       case 0x6:    // right arrow
         cur++;
         if (cur > ind) cur = ind;
         pos = t->getxy();
         if ((pos >> 8) < promptlen + 1) pos = ((promptlen + 1) << 8) | (pos & 0xff);
         if ((pos >> 8) >= promptlen + ind - 1) pos = ((promptlen + ind - 1) << 8) | (pos & 0xff);
         t->gotoxy((pos >> 8) + 1, pos & 0xff);
         t->setcursor(5);
         ch = 0x0;
         continue;
       case 0xe08:  // backspace
         pos = t->getxy();
         if ((pos >> 8) > promptlen)
         {
           cur--;
           ind--;
           t->gotoxy((pos >> 8) - 1, pos & 0xff);
           if ((pos >> 8) <= promptlen) pos = ((promptlen) << 8) | (pos & 0xff);
           if (cur <= 0) cur = 0;
           if (ind <= 0) ind = 0;
           p = cur; 
           while (*(cmdbuf + p) = *(cmdbuf + p + 1)) p++;
           cmdbuf[p++] = '\0';
           t->gotoxy(promptlen, pos & 0xff);
           printf("%s ", cmdbuf);
           t->gotoxy((pos >> 8) - 1, pos & 0xff);
           t->setcursor(5);
         }
         ch = 0x0;
         continue;
       case 0x4:    // del key
         pos = t->getxy();
         if ((pos >> 8) < promptlen + ind)
         {
           ind--;
           p = cur; 
           while (*(cmdbuf + p) = *(cmdbuf + p + 1)) p++;
           cmdbuf[p++] = '\0';
           t->gotoxy(promptlen, pos & 0xff);
           printf("%s ", cmdbuf);
           t->gotoxy((pos >> 8), pos & 0xff);
           t->setcursor(5);
         }
         ch = 0x0;
         continue;
       case 0x1:    // home key
         pos = t->getxy();
         cur = 0;
         t->gotoxy(promptlen, pos & 0xff);
         t->setcursor(5);
         ch = 0x0;
         continue;
       case 0x5:    // end key
         pos = t->getxy();
         cur = ind;
         t->gotoxy(promptlen + ind, pos & 0xff);
         t->setcursor(5);
         ch = 0x0;
         continue;
       case 0x7300: // ctrl-left
         pos = t->getxy();
         while (cur && !isspace(cmdbuf[cur--])) ; // move to the next space symbol to the left
         t->gotoxy(promptlen + cur, pos & 0xff);
         t->setcursor(5);
         ch = 0x0;
         continue;
       case 0x7400: // ctrl-right
         pos = t->getxy();
         while ((cur < ind) && !isspace(cmdbuf[cur++])) ; // move to the next space symbol to the right
         t->gotoxy(promptlen + cur, pos & 0xff);
         t->setcursor(5);
         ch = 0x0;
         continue;
       case 0x11b:  // esc key
         break;   
       case 0x1c0d: // enter key
         cmdbuf[ind++] = '\0';
         return cmdbuf;
       default:
         printf("%c", ch);
         t->setcursor(5);
         cmdbuf[ind++] = (char)(ch & 0xff);
         ch = t->getkey();
         cur = ind;
         continue;
    }
    break;
  } while (1);

  return NULL;
}

int exec_cmd(char *cmd)
{
  return exec_line(cmd);
}

void cmdline(int item, int shift)
{
  int  ii;
  char exitflag = 0;
  char *cmd;
  int  ch;

  //printf("cmdline!\r\n");

  while (1) 
  {
    showpath();
    ch = t->getkey();
    if (ch == 0x11b) break; // esc
    if (cmd = getcmd(ch))
    {
      printf("\r\n%s\r\n", cmd);
      exec_cmd(cmd);
    }
    else 
      break;
  } 

  t->cls();
  state = 0;
  show_background_screen();
  draw_menu(item, shift);
  //for (ii = 0; ii < 0x1000; ii++) ;
}

void menued(int item, int shift)
{
  int  ch;
  int  n;
  int  i;
  char *p;
  script_t *sc;

  n = item + 1;
  sc = menu_first;

  // find a menu item and execute corresponding script
  while (n)
  {
    n--;
    if (!n) 
    { /* (sc->scr, sc->num) */
      p = sc->scr;
      for (i = 0; i < sc->num; i++)
      {
        printf("%s\r\n", p);
        while (*p++) ;
      }
    }
    sc = sc->next;
  }

  while (1)
  {
    ch = t->getkey();
    if (ch == 0x11b) break; // esc
    switch (ch)
    {
      default:
        printf("%c", ch & 0xff);
    }
  }

  t->cls();
  state = 0;
  show_background_screen();
  draw_menu(item, shift);
  //for (ii = 0; ii < 0x1000; ii++) ;
}


int
exec_menu(void)
{
  //int cont = 1;  // continuation flag
  int item;      // selected menu item
  int shift = 0; // horiz. scrolling menu shift

  item = default_item;

  for (;;)
  {
    switch (state)
    {
      case 0: // menu
        do {
          draw_menu(item, shift);
        }   while (get_user_input(&item, &shift));
        if (state) continue; // if we got here by pressing Esc key
        break;                    // otherwise, if Enter key pressed
      case 1: // cmd line
        cmdline(item, shift);
        continue;
      case 2: // menu editor
        menued(item, shift);
        continue;
      default:
        break;
    }
    break;
  } 

  return item;
}

int
exec_script(char *script, int n)
{
  char *line = script;
  int  i;

  for (i = 0; i < n; i++)
  {
    if (!exec_line(line)) return 0;
    // next line
    while (*line++) ;
  }

  return 1;
}

int
exec_cfg(char *cfg)
{
  int rc;
  int item = -1; // menu item number
  char *line, *p;
  script_t *sc;

  config_lines = (char *)m->drives_addr + m->drives_length; // (char *)(0x100000);

  // exec global commands in config file
  // and copy config file to memory as
  // a string table (strings delimited by zeroes)
  // and make script_t structures list for
  // boot scripts and menu items
  menu_len = 0; config_len = 0;
  process_cfg_line = process_cfg_line1;
    
  // clear variable store
  memset(variable_list, 0, sizeof(variable_list));
   
  rc = process_cfg(cfg);
  
  if (!rc) 
    return 0;
  else if (rc == -1)
    panic("exec_cfg(): Error processing config file: ", cfg);

  // starting point 0 instead of 1
  num_items--;

  // show screen header, border and status line
  show_background_screen();

  // show a menu and let the user choose a menu item
  item = exec_menu();

  sc = menu_first;
  t->cls();

  // find a menu item and execute corresponding script
  while (item)
  {
    item--;
    if (!item) rc = exec_script(sc->scr, sc->num);
    if (!rc) return 0;
    sc = sc->next;
  }

  return 1;
}

void
KernelLoader(void)
{
  char *cfg = "/boot/loader/boot.cfg";
  int rc;

  printf("Kernel loader started.\r\n");

  // exec the config file
  rc = exec_cfg(cfg);
  if (!rc)
  {
    printf("Error processing loader config file!\r\n");
  }
  else
  {
    // launch a multiboot kernel

    //printf("entry_addr=0x%x", entry_addr);
    //printf("&mbi=0x%x", m);

    //__asm {
    //  cli
    //  hlt
    //}

    multi_boot();
  }
}

void
cmain(void)
{
  /* Get mbi structure address from pre-loader */
  u_parm(PARM_MBI, ACT_GET, (unsigned int *)&m);
  // init terminal
  t = u_termctl(-1);

  //printf("!!!\r\n");
  //__asm {
  //  cli
  //  hlt
  //}

  KernelLoader();
}
