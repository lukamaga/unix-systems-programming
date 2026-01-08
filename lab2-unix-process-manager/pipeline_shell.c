// Lukash M., Informatika 1 grupe (4 kursas)
// Realizuoti konvejeriai, argumentai atskiriami tarpais(tabais). Kabutes ir escape’ai nepalaikomi
#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

// Ar rezimas interaktyvus ir ar reikia iterpti \n pries prompta
static int g_interactive = 1;
static int g_need_nl_before_prompt = 0;

// Patikrina ar simbolis yra tarpas
static int is_space(char c) {
  return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' ||
         c == '\f';
}

// Nukopijuoja teksta i nauja atminties vieta
static char *xstrdup(const char *s) {
  size_t n = strlen(s) + 1;
  char *p = (char *)malloc(n);  // Paskiriame atminti
  if (!p)
    return NULL;
  memcpy(p, s, n); // Kopijuoja visa teksta
  return p;
}

// Pasalina tarpus nuo pradzios ir galo
static char *trim_ws(char *s) { 
  while (*s && is_space(*s)) // Praleidzia pradinius tarpus
    s++;
  size_t len = strlen(s);  // Apskaiciuoja nauja ilgi
  while (len > 0 && is_space(s[len - 1])) {  // Karpo tarpus is galo
    s[len - 1] = '\0';  // Pakeicia tarpa i pabaigos zenkla
    len--;
  }
  return s;
}

// Isskaido teksta i argv masyva pagal tarpus
static int split_argv(char *segment, char ***argv_out) {
  size_t cap = 8; // pradine vieta rodyklem
  size_t count = 0; // kiek argumentu radome
  char **argv = (char **)malloc(sizeof(char *) * cap);
  if (!argv)
    return -1;

  char *p = segment; // skaitom is pradetu poziciju
  while (*p) {
    while (*p && is_space(*p)) // praleidzia kelius tarpus
      p++;
    if (!*p) // jeigu pabaiga
      break;
    char *start = p;
    while (*p && !is_space(*p)) // eina iki kito tarpo
      p++;
    if (*p) {
      *p = '\0';
      p++;
    }
    if (count + 1 >= cap) {
      cap *= 2;
      char **tmp = (char **)realloc(argv, sizeof(char *) * cap);
      if (!tmp) {
        free(argv);
        return -1;
      }
      argv = tmp;
    }
    argv[count++] = start; // issaugom argumento rodykle
  }

  if (count == 0) { 
    free(argv);
    return -2;
  }

  argv[count] = NULL; //execvp suderinamumui
  *argv_out = argv;
  return (int)count;
}

// Atlaisvina jau suformuotu argv masyvu dali
static void free_commands_partial(char ***commands, int ncmds) {
  if (!commands)
    return;
  for (int i = 0; i < ncmds; i++) {
    free(commands[i]);
  }
}

// Atlaisvina visa komandu struktura pvz argv masyvus ir ju sarasa
static void free_commands(char ***commands, int ncmds) {
  if (!commands)
    return;
  free_commands_partial(commands, ncmds);
  free(commands); // pats sarasas 
}

// Suskaido visa ivesta eilute pagal | i komandu segmentus, segmentas -> argv -> commands
static int parse_pipeline(char *buf, char ****commands_out, int *ncmds_out) {
  size_t seg_cap = 4; // pradine komandu talpa
  size_t seg_count = 0; // kiek komandu turime
  char ***commands = (char ***)malloc(sizeof(char **) * seg_cap);
  if (!commands)
    return -1;

  char *start = buf; // segmento pradzia
  char *p = buf; // einam per eilute

  while (1) {
    if (*p == '|' || *p == '\0') { // segmentas baigesi arba eilutes pabaiga
      char saved = *p;
      *p = '\0'; // uzbaigiame

      char *seg = trim_ws(start); // apkarpo tarpus
      if (seg[0] == '\0') {
        free_commands_partial(commands, (int)seg_count);
        free(commands);
        return -3;
      }

      char **argv = NULL;
      int rc = split_argv(seg, &argv); // pavercia segmenta i argv
      if (rc < 0) {
        free_commands_partial(commands, (int)seg_count);
        free(commands);
        return -1;
      }

      if (seg_count >= seg_cap) {  // praplesti saraso talpa
        seg_cap *= 2;
        char ***tmp =
            (char ***)realloc(commands, sizeof(char **) * seg_cap);
        if (!tmp) {
          free(argv);
          free_commands_partial(commands, (int)seg_count);
          free(commands);
          return -1;
        }
        commands = tmp;
      }
      commands[seg_count++] = argv; // pridedame dar viena komanda

      if (saved == '\0') { // jei ties pabaiga
        break; // baigia cikla
      }

      p++; // pradedame nauja segmenta po |
      start = p;
      continue;
    }
    p++;
  }

  *commands_out = commands; // grazino visas komandas
  *ncmds_out = (int)seg_count;
  return 0;
}

// Vykdo visa pipeline
static int run_pipeline(char ***commands, int ncmds) {
  if (ncmds <= 0)
    return 0;

  int (*pipes)[2] = NULL; // masyvas is  pipe'u
  if (ncmds > 1) {
    pipes = (int(*)[2])malloc(sizeof(int[2]) * (size_t)(ncmds - 1));
    if (!pipes) {
      perror("malloc");
      return 1;
    }
    for (int i = 0; i < ncmds - 1; i++) {
      if (pipe(pipes[i]) == -1) {
        perror("pipe");
        // uzdarome jau sukurtus
        for (int j = 0; j < i; j++) {
          close(pipes[j][0]);
          close(pipes[j][1]);
        }
        free(pipes);
        return 1;
      }
    }
  }

  pid_t *pids = (pid_t *)malloc(sizeof(pid_t) * (size_t)ncmds);
  if (!pids) {
    perror("malloc");
    if (pipes) {
      for (int i = 0; i < ncmds - 1; i++) {
        close(pipes[i][0]);
        close(pipes[i][1]);
      }
      free(pipes);
    }
    return 1;
  }

  int i;
  for (i = 0; i < ncmds; i++) {
    pid_t pid = fork();
    if (pid < 0) { // fork klaida
      perror("fork");
      break;
    } else if (pid == 0) { // vaikas
      signal(SIGINT, SIG_DFL); // vaikas turi nutraukti pagal nutylejima
      signal(SIGQUIT, SIG_DFL);

      if (i > 0) { // stdin: prijungiame is ankstesnio pipe
        if (dup2(pipes[i - 1][0], STDIN_FILENO) == -1) {
          perror("dup2");
          _exit(1);
        }
      }
      if (i < ncmds - 1) { // stdout: prijungiame i kita pipe
        if (dup2(pipes[i][1], STDOUT_FILENO) == -1) {
          perror("dup2");
          _exit(1);
        }
      }
      // Vaikas uzdaro visus nereikalingus pipe galus
      if (pipes) { 
        for (int j = 0; j < ncmds - 1; j++) {
          close(pipes[j][0]);
          close(pipes[j][1]);
        }
      }

      execvp(commands[i][0], commands[i]);

      perror(commands[i][0]);
      _exit(127);
    } else {
      pids[i] = pid; // tevui issaugomas pid
    }
  }
  // Tevas: uzdaro visus pipe galus
  if (pipes) {
    for (int j = 0; j < ncmds - 1; j++) {
      close(pipes[j][0]);
      close(pipes[j][1]);
    }
    free(pipes);
  }

  int last_status = 0; // paskutinio proceso rezultatas
  int created = (i < ncmds) ? i : ncmds;  // kiek vaiku  sukurta

  for (int k = 0; k < created; k++) { // Laukiami vaikai pagal eiles tvarka
    int status = 0;
    if (waitpid(pids[k], &status, 0) == -1) {
      perror("waitpid");
      continue;
    }
    if (k == created - 1) { // Fiksuojamas paskutinio proceso kodas
      if (WIFEXITED(status)) {
        last_status = WEXITSTATUS(status);
      } else if (WIFSIGNALED(status)) {
        last_status = 128 + WTERMSIG(status);
      } else {
        last_status = 1;
      }
    }
  }

  free(pids);
  return last_status;
}

static void print_prompt(void) { // Isveda paprasta prompt
  // Prompt rodome tik tada, kai STDIN/STDOUT yra TTY
  if (!g_interactive)
    return;

  // Jei praeita komanda baigėsi be \n, iterpama nauja eilute
  if (g_need_nl_before_prompt) {
    fputc('\n', stdout);
    g_need_nl_before_prompt = 0;
  }

  fputs("mini-sh> ", stdout);
  fflush(stdout);
}

int main(void) { 
  // Tevas ignoruoja SIGINT/SIGQUIT, kad Ctrl-C nenuzudytu viso shell
  signal(SIGINT, SIG_IGN);
  signal(SIGQUIT, SIG_IGN);

  g_interactive = isatty(STDIN_FILENO) && isatty(STDOUT_FILENO);

  char *line = NULL; // getline buferis
  size_t cap = 0; // getline talpa

  while (1) { 
    print_prompt();

    errno = 0;
    ssize_t nread = getline(&line, &cap, stdin); // skaitome eilute
    if (nread == -1) {
      if (errno == EINTR) { // jei pertrauke signalas tai  bando dar karta
        clearerr(stdin);
        continue;
      }
      if (feof(stdin)) { // jei EOF baigiame
        if (g_interactive)
          fputc('\n', stdout);
      } else {
        perror("getline");
      }
      break;
    }

    if (nread > 0 && line[nread - 1] == '\n') { // nukerpa '\n'
      line[nread - 1] = '\0';
    }

    char *trimmed = trim_ws(line);
    if (trimmed[0] == '\0') { // jei tuscia eilute tai praleidziam
      continue;
    }

    if (strcmp(trimmed, "exit") == 0) { // baigti
      break;
    }

    char *work = xstrdup(trimmed); //darbine kopija
    if (!work) {
      perror("strdup");
      continue;
    }
    // Suskaido i komandas pagal |
    char ***commands = NULL;
    int ncmds = 0;
    int prc = parse_pipeline(work, &commands, &ncmds);
    if (prc == -3) {
      fprintf(stderr, "Klaida: netinkama null komanda (neteisingas '|').\n");
      free(work);
      continue;
    } else if (prc != 0) {
      fprintf(stderr, "Klaida: nepavyko apdoroti komandos.\n");
      free(work);
      continue;
    }

    (void)run_pipeline(commands, ncmds);
    //iterpti \n pries kita prompta
    if (g_interactive)
     g_need_nl_before_prompt = 1;

    // valo strukturas
    free_commands(commands, ncmds);
    free(work);
  }

  free(line);
  return 0;
}
