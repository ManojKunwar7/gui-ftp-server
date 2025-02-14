#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <curses.h>
#include <assert.h>
#include <string.h>
#include <curl/curl.h>
#include <errno.h>
#include <sys/stat.h>

#define SV_IMPLEMENTATION
#include "./sv.h"

#define UP 119
#define DOWN 115
#define LEFT 97
#define RIGHT 100
#define SELECT 10
#define BACK 2
#define ESC 5

#define dir_color 1

typedef struct Menu_Item
{
  int index;
  bool is_dir;
  char *permission;
  char *date;
  size_t file_size;
  bool name;
  char *menu_name;
  size_t count;
} Menu_Item;

typedef struct Menu_Items
{
  Menu_Item *item;
  size_t count;
  size_t capacity;
} Menu_Items;

typedef struct Path
{
  char *path;
} Path;

typedef struct PWD
{
  Path *path;
  size_t count;
  size_t capacity;
} PWD;

typedef struct Chunk
{
  char *memory;
  size_t size;
} Chunk;

typedef struct Output_file
{
  char *file_name;
  FILE *stream;
} Output_file;

int build_menu_from_str(char *ftp_url, Menu_Items *menu);

size_t sv_str_to_long(char *str)
{
  char *endptr;
  long int num = strtol(str, &endptr, 10);
  if (*endptr != '\0')
  {
    printf("Invalid character: %c\n", *endptr);
    return 0;
  }
  return num ? (size_t)num : 0;
}

char *str_append(char *str1, char *str2)
{
  char *buffer = malloc((strlen(str1) + strlen(str2) + 5) * sizeof(char));
  sprintf(buffer, "%s%s", str1, str2);
  return buffer;
}

size_t allocate_pwd(PWD *pwd)
{
  if (pwd->count >= pwd->capacity)
  {
    if (pwd->capacity == 0)
    {
      assert(pwd->path == NULL);
      pwd->capacity = 100;
    }
    else
    {
      pwd->capacity *= 2;
    }
    pwd->path = realloc(pwd->path, sizeof(char) * pwd->capacity);
  }
  return pwd->count++;
}

Path *pwd_at(PWD *pwd, size_t index)
{
  assert(index < pwd->count);
  return &pwd->path[index];
}

size_t pwd_splice(PWD *pwd, size_t index)
{
  assert(index <= pwd->count);
  // ! Throw an error
  pwd->count -= 1;
  return pwd->count;
}

char *get_pwd(PWD *pwd)
{
  char *prefix = "";
  for (size_t i = 0; i < pwd->count; i++)
  {
    Path *pwd_path = pwd_at(pwd, i);
    char *buffer = malloc((strlen(pwd_path->path) + strlen(prefix) + 5) * sizeof(char));
    if (i == 0)
      sprintf(buffer, "//%s", pwd_path->path);
    else
      sprintf(buffer, "%s/%s", prefix, pwd_path->path);
    // prefix = str_append(buffer, pwd_path->path);
    prefix = buffer;
  }
  return prefix;
}

size_t allocate_menu(Menu_Items *menu)
{
  if (menu->count >= menu->capacity)
  {
    if (menu->capacity == 0)
    {
      assert(menu->item == NULL);
      menu->capacity = 30;
    }
    else
    {
      menu->capacity *= 2;
    }
    menu->item = realloc(menu->item, sizeof(Menu_Item) * menu->capacity);
  }
  return menu->count++;
}

Menu_Item *menu_item_at(Menu_Items *menu, size_t index)
{
  assert(index < menu->count);
  return &menu->item[index];
}

static size_t download_file_writer(void *buffer, size_t size, size_t nmemb, void *stream)
{
  Output_file *out = (struct Output_file *)stream;
  if (!out->stream)
  {
    /* open file for writing */
    out->stream = fopen(out->file_name, "wb");
    if (!out->stream)
      return 0; /* failure, cannot open file to write */
  }
  return fwrite(buffer, size, nmemb, out->stream);
}

int download_file_from_ftp(Output_file *file, char *ftp_url)
{
  CURL *curl;
  CURLcode res;
  printf("\n ftp_url %s", ftp_url);
  curl_global_init(CURL_GLOBAL_DEFAULT);

  curl = curl_easy_init();
  if (curl)
  {
    /*
     * You better replace the URL with one that works!
     */
    curl_easy_setopt(curl, CURLOPT_URL, ftp_url);
    /* Define our callback to get called when there is data to be written */
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, download_file_writer);
    /* Set a pointer to our struct to pass to the callback */
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, file);

    /* Switch on full protocol/debug output */
    // curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);

    res = curl_easy_perform(curl);

    /* always cleanup */
    curl_easy_cleanup(curl);

    if (CURLE_OK != res)
    {
      /* we failed */
      fprintf(stderr, "curl told us %d\n", res);
    }
  }

  // if (file->stream)
  //   fclose(file->stream); /* close the local file */

  curl_global_cleanup();
  return 0;
}

int build_graphical_menu(char *ftp_url, Menu_Items *menu, PWD *pwd, size_t selection)
{
  int ch;
  initscr();                        // intialize screen
  noecho();                         // don't print the typed character
  curs_set(0);                      // hide cursor
  WINDOW *win = newwin(0, 0, 0, 0); // start a window
  clear();
  refresh();
  box(win, 0, 0); // add a border in window (0, 0) means default border
  // wrefresh(win);  // refresh the passed window to show changes
  init_pair(dir_color, COLOR_CYAN, COLOR_BLUE);
  mvwprintw(win, 1, 1, "    %5s %2s %10s %10s %10s", "Permission", "Type", "Size", "Date", "Name");
  for (size_t i = 0; i < menu->count; i++)
  {
    Menu_Item *menu_item = menu_item_at(menu, i);
    char *isDir = menu_item->is_dir ? "directory" : "file     ";
    if (selection == i + 1)
    {
      // if(menu_item->is_dir) attron(COLOR_PAIR(dir_color));
      mvwprintw(win, i + 3, 1, "--> %s %s %5ld %2s %s", menu_item->permission, isDir, menu_item->file_size, menu_item->date, menu_item->menu_name);
      // if(menu_item->is_dir) attroff(COLOR_PAIR(dir_color));
    }
    else
    {
      // if(menu_item->is_dir) attron(COLOR_PAIR(dir_color));
      mvwprintw(win, i + 3, 1, "    %s %s %5ld %2s %s", menu_item->permission, isDir, menu_item->file_size, menu_item->date, menu_item->menu_name);
      // if(menu_item->is_dir) attroff(COLOR_PAIR(dir_color));
    }
  }
  refresh();
  wrefresh(win);

  // getch();
  while ((ch = wgetch(win)))
  {
    // printf("-->> %d\n", ch);

    //! Quit Program
    if (ch == ESC)
    {
      break;
    }
    switch (ch)
    {
    case UP:
    {
      size_t prev = selection - 1;
      if (prev == 0)
        prev = selection;
      build_graphical_menu(ftp_url, menu, pwd, prev);
      break;
    }
    case DOWN:
    {
      size_t next = selection + 1;
      if (next > menu->count)
        next = selection;
      build_graphical_menu(ftp_url, menu, pwd, next);
      break;
    }
    case SELECT:
    {
      // ! Can use free
      Menu_Item *menu_item = menu_item_at(menu, selection - 1);
      if (menu_item->file_size > 0)
      {
        char *selected_name = menu_item->menu_name;
        size_t pwd_index = allocate_pwd(pwd);
        Path *pwd_path = pwd_at(pwd, pwd_index);
        // ! add entry on pwd
        pwd_path->path = selected_name;
        char *pwd_prefix = get_pwd(pwd);
        char *joined_url = str_append(ftp_url, pwd_prefix);
        if (menu_item->is_dir)
        {
          // ! Rebuild the menu
          if (menu->item)
          {
            free(menu->item);
            // Menu_Items *new_menu = {0};
            menu->item = NULL;
            menu->count = 0;
            menu->capacity = 0;
          }
          char *dir = "/";
          char *new_url = str_append(joined_url, dir);
          build_menu_from_str(new_url, menu);
          build_graphical_menu(ftp_url, menu, pwd, 1);
          if (new_url)
            free(new_url);
        }
        else
        {
          // ! Download file
          Output_file file = {
              .stream = NULL,
              .file_name = menu_item->menu_name,
          };
          download_file_from_ftp(&file, joined_url);
          if (file.stream)
            fclose(file.stream); /* close the local file */
        }
        if (pwd_prefix)
          free(pwd_prefix);
        if (joined_url)
          free(joined_url);
      }
      break;
    }
    case BACK:
    {
      // ! Rebuild the menu
      if (pwd->count > 0)
        pwd_splice(pwd, pwd->count);
      // ! remove entry on pwd
      char *pwd_prefix = get_pwd(pwd);
      char *joined_url = str_append(ftp_url, pwd_prefix);
      char *dir = "/";
      char *new_url = str_append(joined_url, dir);
      // ! Rebuild the menu
      if (menu->item)
      {
        free(menu->item);
        menu->item = NULL;
        menu->count = 0;
        menu->capacity = 0;
      }
      build_menu_from_str(new_url, menu);
      build_graphical_menu(ftp_url, menu, pwd, 1);
      if (new_url != NULL)
        free(new_url);
      if (pwd_prefix != NULL)
        free(pwd_prefix);
      break;
    }
    default:
      build_graphical_menu(ftp_url, menu, pwd, selection);
      break;
    }
  }
  echo(); // don't print the typed character
  curs_set(1);
  endwin();
  return 0;
}

void get_credentials(char *username, char *password, char *ftp_url, char *server_name)
{
  printf("(200) Enter your username:- ");
  fgets(username, sizeof(char) * 200, stdin);
  username[strcspn(username, "\n")] = 0;
  printf("(200) Enter your password:- ");
  fgets(password, sizeof(char) * 200, stdin);
  password[strcspn(password, "\n")] = 0;
  printf("(200) Enter your server_name:- ");
  fgets(server_name, sizeof(char) * 200, stdin);
  server_name[strcspn(server_name, "\n")] = 0;
  sprintf(ftp_url, "ftp://%s:%s@%s", username, password, server_name);
}

static size_t write_directory(void *data, size_t size, size_t nmemb, void *userptr)
{
  size_t real_size = size * nmemb;
  Chunk *mem = (Chunk *)userptr;
  char *ptr = realloc(mem->memory, mem->size + real_size + 1);
  if (!ptr)
  {
    fprintf(stderr, "%s System out of memory", strerror(errno));
    return 0;
  }
  mem->memory = ptr;
  memcpy(&(mem->memory[mem->size]), data, real_size);
  mem->size += real_size;
  mem->memory[mem->size] = 0;
  return real_size;
}

void curl_connection(char *ftp_url, Chunk *buffer, bool *exit)
{
  Chunk chunk;
  chunk.memory = malloc(1);
  chunk.size = 0;
  curl_global_init(CURL_GLOBAL_DEFAULT);
  CURL *curl = curl_easy_init();
  if (curl)
  {
    curl_easy_setopt(curl, CURLOPT_URL, ftp_url);
    // ! Get folder List
    /* send all data to this function  */
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_directory);

    /* we pass our 'chunk' struct to the callback function */
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&chunk);
    CURLcode res = curl_easy_perform(curl);
    /* always cleanup */
    curl_easy_cleanup(curl);
    // printf("res %d \n", res);
    if (CURLE_OK != res)
    {
      /* we failed */
      fprintf(stderr, "curl told us %d\n", res);
      if (res == 7)
      {
        *exit = true;
        fprintf(stderr, "ERROR: Could not connect to host!\n");
        buffer = NULL;
      }
      if (res == 6)
      {
        *exit = true;
        fprintf(stderr, "ERROR: There was a problem resolving the hostname!\n");
        buffer = NULL;
      }
      if (res == 67)
      {
        *exit = true;
        fprintf(stderr, "ERROR: Incorrect username, password, or server name was not accepted and curl failed to log in!\n");
        buffer = NULL;
      }
    }
  }
  // return &chunk;
  if (buffer != NULL)
    *buffer = chunk;
}

void slurp_file(const char *file_name)
{
  char *buffer = NULL;
  FILE *read_tmp = fopen(file_name, "rb");
  if (read_tmp == NULL)
  {
    goto error;
  }
  if (fseek(read_tmp, 0, SEEK_END) < 0)
  {
    goto error;
  }
  long m = ftell(read_tmp);
  buffer = malloc(sizeof(char) * m);
  if (buffer == NULL)
  {
    goto error;
  }
  if (fseek(read_tmp, 0, SEEK_SET) < 0)
  {
    goto error;
  }
  size_t n = fread(buffer, 1, m, read_tmp);
  assert(n == (size_t)m);
  if (ferror(read_tmp))
  {
    goto error;
  }
  // if(fopen(read_tmp))
  fclose(read_tmp);
  remove(file_name);
  // printf("This is data:- \n%s\n", buffer);
  return;
error:
  if (read_tmp)
    fclose(read_tmp);
  if (buffer)
    free(buffer);
  remove(file_name);
}

/* NOTE: if you want this example to work on Windows with libcurl as a DLL,
   you MUST also provide a read callback with CURLOPT_READFUNCTION. Failing to
   do so might give you a crash since a DLL may not use the variable's memory
   when passed in to it from an app like this. */
static size_t read_callback(char *ptr, size_t size, size_t nmemb, void *stream)
{
  unsigned long nread;
  /* in real-world cases, this would probably get this data differently
     as this fread() stuff is exactly what the library already would do
     by default internally */
  size_t retcode = fread(ptr, size, nmemb, stream);
  if (retcode > 0)
  {
    nread = (unsigned long)retcode;
    fprintf(stderr, "*** We read %lu bytes from file\n", nread);
  }
  return retcode;
}

int upload_file(const char *file_name, const char *ftp_url, const char *dist)
{
  printf("file_name :- %s \n", file_name);
  //! File handling
  FILE *file;
  struct stat file_info;
  unsigned long fsize;

  struct curl_slist *headerlist = NULL;
  // // upload file name
  // char buf_1[] = "RNFR ";
  // // rename file name
  // char buf_2[] = "RNTO ";
  // sprintf(buf_1, "%s", file_name);
  // sprintf(buf_2, "%s", file_name);

  /* get the file size of the local file */
  if (stat(file_name, &file_info))
  {
    fprintf(stderr, "Couldn't open '%s': %s\n", file_name, strerror(errno));
    return 1;
  }
  fsize = (unsigned long)file_info.st_size;
  printf("Local file size: %lu bytes.\n", fsize);
  /* * get a FILE * of the same file */
  file = fopen(file_name, "rb");
  if (!file)
    return 2;
  // ! Make Connection
  // char *REMOTE_URL = {0};
  struct curl_slist *cmdlist = NULL;
  char REMOTE_URL[1024 * 10];
  if (strlen(dist))
  {
    sprintf(REMOTE_URL, "%s/%s/%s", ftp_url, dist, file_name);
  }
  else
  {
    sprintf(REMOTE_URL, "%s/%s", ftp_url, file_name);
  }
  printf("This is url %s\n", REMOTE_URL);

  /* In Windows, this inits the Winsock stuff */
  // curl_global_init(CURL_GLOBAL_ALL);
  /* get a curl handle */
  CURL *curl = curl_easy_init();
  if (curl)
  {
    /* build a list of commands to pass to libcurl */
    // headerlist = curl_slist_append(headerlist, buf_1);
    // headerlist = curl_slist_append(headerlist, buf_2);

    /* pass in the FTP commands to run before the transfer */
    curl_easy_setopt(curl, CURLOPT_QUOTE, cmdlist);

    /* we want to use our own read function */
    curl_easy_setopt(curl, CURLOPT_READFUNCTION, read_callback);

    /* enable uploading */
    curl_easy_setopt(curl, CURLOPT_UPLOAD, 1L);

    /* specify target */
    curl_easy_setopt(curl, CURLOPT_URL, REMOTE_URL);

    /* pass in that last of FTP commands to run after the transfer */
    // curl_easy_setopt(curl, CURLOPT_POSTQUOTE, headerlist);

    /* now specify which file to upload */
    curl_easy_setopt(curl, CURLOPT_READDATA, file);

    /* Set the size of the file to upload (optional).  If you give a *_LARGE
       option you MUST make sure that the type of the passed-in argument is a
       curl_off_t. If you use CURLOPT_INFILESIZE (without _LARGE) you must
       make sure that to pass in a type 'long' argument. */
    curl_easy_setopt(curl, CURLOPT_INFILESIZE_LARGE, (curl_off_t)fsize);

    /* Now run off and do what you have been told! */
    CURLcode res = curl_easy_perform(curl);
    printf("%u \n", res);
    /* Check for errors */
    if (res != CURLE_OK)
    {
      if(res == 21){
        fprintf(stderr, "Maybe no such directory %s\n", curl_easy_strerror(res));
      }
      fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
    }

    /* clean up the FTP commands list */
    curl_slist_free_all(headerlist);

    /* always cleanup */
    curl_easy_cleanup(curl);
  }
  fclose(file); /* close the local file */
  curl_global_cleanup();
  // if(REMOTE_URL)
  //   free(REMOTE_URL);
  return 0;
}

int build_menu_from_str(char *ftp_url, Menu_Items *menu)
{
  // ! Make connection
  Chunk buffer = {0};
  bool exit = false;
  curl_connection(ftp_url, &buffer, &exit);
  if (exit)
    return 0;
  if (buffer.memory == NULL)
  {
    goto error;
  }
  if (buffer.size <= 0)
  {
    size_t index = allocate_menu(menu);
    Menu_Item *menu_item = menu_item_at(menu, index);
    menu_item->index = 1;
    menu_item->count = 0;
    menu_item->date = "(no-data)";
    menu_item->file_size = 0;
    menu_item->menu_name = "(no-data)";
    menu_item->is_dir = false;
    menu_item->permission = "(no-data)";

    if (buffer.memory)
      free(buffer.memory);
    return 0;
  }
  String_View dir_str = {
      .count = buffer.size,
      .data = buffer.memory,
  };

  for (size_t i = 0; i < dir_str.count; i++)
  {
    String_View line = sv_trim(sv_chop_by_delim(&dir_str, '\n'));
    size_t count = line.count;
    size_t index = allocate_menu(menu);
    Menu_Item *menu_item = menu_item_at(menu, index);
    menu_item->index = i + 1;
    size_t jmp_count = 0;
    for (size_t j = 0; j < count; ++j)
    {
      String_View part = sv_trim(sv_chop_by_delim(&line, ' '));
      if (!part.count)
        continue;
      // ! Review this
      if (jmp_count == 0)
      {
        menu_item->permission = malloc((part.count + 5) * sizeof(char));
        sprintf(menu_item->permission, SV_Fmt, SV_Arg(part));
        jmp_count++;
      }
      else if (jmp_count == 1)
      {
        char *temp = malloc((part.count + 5) * sizeof(char));
        sprintf(temp, SV_Fmt, SV_Arg(part));
        menu_item->is_dir = *temp == '2' ? true : false;
        free(temp);
        jmp_count++;
      }
      else if (jmp_count == 2 || jmp_count == 3)
        jmp_count++;
      else if (jmp_count == 4)
      {
        char *data = malloc((part.count + 5) * sizeof(char));
        sprintf(data, SV_Fmt, SV_Arg(part));
        menu_item->file_size = sv_str_to_long(data);
        free(data);
        jmp_count++;
      }
      else if (jmp_count == 5 || jmp_count == 6 || jmp_count == 7)
      {
        if (jmp_count == 6 || jmp_count == 7)
        {
          char *temp = malloc((part.count + 5) * sizeof(char));
          sprintf(temp, SV_Fmt, SV_Arg(part));
          menu_item->date = realloc(menu_item->date, (strlen(menu_item->date) + strlen(temp) + 5) * sizeof(char));
          // str_append(menu_item->date, temp);
          sprintf(menu_item->date, "%s %s", menu_item->date, temp);
          free(temp);
        }
        else
        {
          menu_item->date = malloc((part.count + 5) * sizeof(char));
          sprintf(menu_item->date, SV_Fmt, SV_Arg(part));
        }
        jmp_count++;
      }
      else if (jmp_count == 8)
      {
        menu_item->menu_name = malloc((part.count + 5) * sizeof(char));
        sprintf(menu_item->menu_name, SV_Fmt, SV_Arg(part));
        jmp_count++;
      }
    }
  }

  if (buffer.memory)
    free(buffer.memory);
  return 0;
error:
  if (buffer.memory)
    free(buffer.memory);
  if (ftp_url)
    free(ftp_url);
  return 0;
}

int main(int argc, char **argv)
{
  if (argc < 0)
    return 0;
  // ! Make connection
  char *username = malloc(sizeof(char) * 200);
  char *password = malloc(sizeof(char) * 200);
  char *ftp_url = malloc(sizeof(char) * 200);
  char *remote_url = malloc(sizeof(char) * 200);

  get_credentials(username, password, ftp_url, remote_url);
  if (argc > 1 && strcmp("upload", argv[1]) == 0)
  {
    if (argc < 3)
    {
      if (argv[2] == NULL)
      {
        fprintf(stderr, "%s Please specify a file to upload!\n", strerror(errno));
      }
      if (argv[3] == NULL)
      {
        fprintf(stderr, "%s destination folder should be specifed in this format 'folder/subfolder' !\n", strerror(errno));
      }
      return 0;
    }
    // ! Write upload function
    const char *file_name = argv[2];
    const char *dist = argv[3] ? argv[3] : "";
    printf("this is dist %s \n", dist);
    upload_file(file_name, ftp_url, dist);
  }
  else
  {
    Menu_Items menu = {0};
    PWD pwd = {0};
    build_menu_from_str(ftp_url, &menu);
    if (!menu.count)
      return 0;
    // ! Show menu
    build_graphical_menu(ftp_url, &menu, &pwd, 1);
    printf("/n comes ");
    // ! Free vaiables
    if (menu.item)
      free(menu.item);
    if (username)
      free(username);
    if (password)
      free(password);
    if (remote_url)
      free(remote_url);
    if (ftp_url)
      free(ftp_url);
    if (pwd.path)
      free(pwd.path);
  }
  return 0;
}
