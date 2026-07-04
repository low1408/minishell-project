#ifndef MINISHELL_H
# define MINISHELL_H

# include <unistd.h>
# include <stdio.h>
# include <stdlib.h>
# include <fcntl.h>
# include <readline/readline.h>
# include <readline/history.h>
# include <sys/types.h>
# include <signal.h>
# include <errno.h>
# include <sys/wait.h>
# include <limits.h>

# define BUFFER_SIZE 4096
# define APP "minishell"

extern volatile sig_atomic_t g_signal;

typedef enum e_qstate
{
	Q_NONE,
	Q_SQUOTE,
	Q_DQUOTE
}	t_qstate;

typedef enum e_errcode
{
	ERR_MALLOC = -1,
	ERR_QUOTE = -2,
	ERR_SYNTAX = -3,
	ERR_CMDNEXEC = -4,
	ERR_CMDNFOUND = -5,
	ERR_PIPE = -6,
	ERR_FORK = -7,
	ERR_CD = -8,
	ERR_REDIR = -9,
	ERR_INVAL = -10,
}	t_errcode;

/**
 * @brief Represents standard POSIX exit status codes for a process.
 *
 * These codes are returned to the operating system upon process termination
 * to indicate the outcome of the execution.
 */
typedef enum e_exitcode
{
	EX_OK = 0,         /**< Program executed successfully. */
	EX_ERR = 1,        /**< General catchall for minor/generic errors. */
	EX_SYNTAX = 2,
	EX_CMD_NEXEC    = 126, /**< Command invoked but is not executable. */
	EX_CMD_NOTFOUND = 127, /**< Command cannot be found in PATH. */
	EX_SIG_BASE     = 128  /**< Base offset for fatal signal terminations. */
}	t_exitcode;

# include "ast.h"
# include "parser.h"
# include "envp.h"

typedef struct s_app
{
	t_tokensll	*tokensll;
	t_ast_node	*ast;
	t_env		*env_list;
	char		**envp;
	int			exitcode;
}	t_app;

int		update_env_array(t_app *app);

# include "lexer.h"
# include "expander.h"
# include "exec.h"
# include "builtin.h"

/* === utils_exit.c === */
/**
 * @brief Terminate the program immediately.
 *
 * Intended for unrecoverable error paths where cleanup is either not
 * possible or not required.
 */
void	hardexit(void);
void	setexit(t_app *app, t_exitcode code);
void		setexecerrno(t_app *app);


/* === utils_mem.c === */
/**
 * @brief Copy memory from src to dest.
 * @param dest Destination buffer.
 * @param src Source buffer.
 * @param nbyte Number of bytes to copy.
 * @return Destination pointer.
 */
void	*ft_memcpy(void *dest, const void *src, size_t nbyte);
/**
 * @brief Fill the first nbyte bytes of dest with value c.
 * @param dest Destination buffer.
 * @param c Fill value (converted to unsigned char).
 * @param nbyte Number of bytes to fill.
 * @return dest, or NULL if dest is NULL.
 */
void	*ft_memset(void *dest, int c, size_t nbyte);
/**
 * @brief Reallocate a string buffer to a new size.
 * @param old Existing buffer (may be NULL).
 * @param old_size Size of the existing buffer in bytes.
 * @param new_size Desired buffer size in bytes.
 * @return Newly allocated buffer, or NULL on failure.
 */
char	*ft_realloc(char *old, size_t old_size, size_t new_size);
/**
 * @brief Allocate a NULL-terminated char** list of size+1 slots.
 * @param size Number of usable string slots.
 * @return Pointer to allocated list, or NULL on failure.
 */
char	**ft_calloclst(int size);
/**
 * @brief Free a NULL-terminated char** list and all its strings.
 * @param lst List to free (may be NULL).
 */
void	freelst(char **lst);


/* === utils_print.c === */
/**
 * @brief Write a string to a file descriptor.
 * @param s Null-terminated string to write.
 * @param fd File descriptor to write to.
 */
void	ft_putstr_fd(char *s, int fd);
void	print_tokensll(t_tokensll *tokensll);
void	errmsg(const char *prefix, const char *arg, const char *msg);


/* === utils_printerr.c === */
void	printerr_syscall(t_errcode code);
void	printerr_syntax(char *tokval);
void	printerr_quotes(void);
void	printerr_redir(char *filename);
void	printerr_cmdnfound(char *cmd);
void	ft_perror(char *msg);


/* === utils_printerr_builtin.c === */
void	printerr_cd(char *filename);


/* === utils_str.c === */
/**
 * @brief Compute the length of a null-terminated string.
 * @param str Input string.
 * @return Length excluding the null terminator.
 */
size_t	ft_strlen(const char *str);
/**
 * @brief Duplicate up to len characters into a new string.
 * @param str Source buffer.
 * @param len Maximum number of characters to copy.
 * @return Newly allocated string, or NULL on failure.
 */
char	*ft_strndup(char *str, int len);
/**
 * @brief Compare two null-terminated strings.
 * @param s1 First string.
 * @param s2 Second string.
 * @return Difference between the first differing characters.
 */
int		ft_strcmp(const char *s1, const char *s2);
char	*ft_strdup(const char *s);
char	*ft_strjoin(const char *s1, const char *s2);
int		ft_strncmp(const char *s1, const char *s2, size_t n);
char	*ft_strchr(const char *s, int c);
char	*ft_itoa(int n);


/* === utils_validator.c === */
/**
 * @brief Check if a character is considered whitespace.
 * @param ch Character code to test.
 * @return Nonzero if whitespace, zero otherwise.
 */
int		iswhitespace(int ch);
/**
 * @brief Check if a character is a special shell symbol.
 * @param ch Character code to test.
 * @return Nonzero if special, zero otherwise.
 */
int		isspecialsym(int ch);
/**
 * @brief Check if str contains character ch.
 * @param str String to search.
 * @param ch Character to look for.
 * @return 1 if found, 0 otherwise.
 */
int		ft_strhaschr(char *str, char ch);
int		is_numeric(const char *s, long long *val);


/* === signals.c === */
void	handlesig_prompt(int sig);
void	signals_at_prompt(void);
void	signals_ignore(void);
void	signals_default(void);
void	handle_sigint_in_main(t_app *app);

#endif
