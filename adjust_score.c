/** @file adjust_score.c
 *  @brief Functions for amending a player's score in the
 *  `/var/lib/curdle/scores` file.
 *
 *  Contains adjust_score_file function and supporting functions
 *  used by the function.
 */

#define _POSIX_C_SOURCE 200809L
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <errno.h>
#include <sys/types.h>
#include <unistd.h>
#include <ctype.h>

#define SIZE 40

/** Size of a field(name or score) in a line of the `scores`
 * file.
 */
#define FIELD_SIZE 10

/** Size of a line in the `scores file.
 */
#define REC_SIZE (FIELD_SIZE * 2 + 1)

/** Lower bound of score, score is 10 character long
 *  including the minus sign.
 */
#define SCORE_LOW_BOUND -999999999
#define FILENAME "file0"

typedef enum
{
    SCORE_VALID,
    SCORE_OVERFLOW,
    SCORE_UNDERFLOW,
    SCORE_INVALID
} score_errno;

/** Store error into message collection.
 *  Prints the current error message stored.
 *
 *  \param error is the error message to store into collection.
 *  \return message if memory allocation has no error, EXIT_FAILURE if there is error.
 */
char *handle_message(char *error)
{
    char *message = malloc(sizeof(char) * SIZE);
    if (message == NULL)
    {
        free(message);
        fprintf(stderr, "Failed to allocate memory\n");
        exit(EXIT_FAILURE);
    }
    if (strncpy(message, error, SIZE) == NULL)
    {
        free(message);
        fprintf(stderr, "Failed to copy message\n");
        exit(EXIT_FAILURE);
    }
    return message;
}

/** Check if the sum of current score
 *  and score_to_add is within the range of INT_MAX
 *  and SCORE_LOW_BOUND.
 *
 *  \param old current score inside file.
 *  \param new score_to_add.
 *  \return 1 if sum is within the bounds, 0 if not.
 */
int check_sum(long old, long new)
{
    long sum = old + new;
    if (sum > INT_MAX)
    {
        return 0;
    }
    else if (sum < SCORE_LOW_BOUND)
    {
        return 0;
    }
    return 1;
}

/** Convert score inside file from char *
 *  to int
 *
 *  \param score_line char * containing the score.
 *  \param score address to store score.
 *  \return SCORE_VALID if the score is valid and within bounds,
 *  return SCORE_INVALID if score is not in the right format,
 *  SCORE_OVERFLOW if score is above LONG_MAX
 *  SCORE_UNDERFLOW if score is below LONG_MIN
 */
score_errno score_strtol(int *score, char *score_line)
{
    if (score_line == NULL || score_line[0] == '\0' || isspace(score_line[0]))
    {
        return SCORE_INVALID;
    }

    char *endptr;
    errno = 0;
    long score_long = strtol(score_line, &endptr, 10);
    if (score_long > INT_MAX || (errno == ERANGE && score_long == LONG_MAX))
    {
        return SCORE_OVERFLOW;
    }
    if (score_long < SCORE_LOW_BOUND || (errno == ERANGE && score_long == LONG_MIN))
    {
        return SCORE_UNDERFLOW;
    }
    if (*endptr != '\0')
    {
        return SCORE_INVALID;
    }

    *score = score_long;
    return SCORE_VALID;
}

/** Check if the score in line_buf is valid
 *  Read characters in line
 *  Starts reading where name ends, line_buf[10],
 *  Converts char *score into int by calling score_strtol.
 *
 *  \param line_buf the current line in file
 *  \return score_int if score is in the valid format,
 *  INT_MIN if not.
 */
int get_score(char *line_buf)
{
    if (line_buf[REC_SIZE - 1] != 0x0A)
    {
        return INT_MIN;
    }

    int score_int = 0;
    char score[FIELD_SIZE + 1];
    for (int i = 10; i < REC_SIZE; i++)
    {
        score[i - 10] = line_buf[i];
    }
    score[FIELD_SIZE] = '\0';
    if (score_strtol(&score_int, score) != SCORE_VALID)
    {
        printf("string to in fails\n");
        return INT_MIN;
    }
    return (int)score_int;
}

/** Check if the name in line_buf is valid
 *  Read characters in line
 *  Starts reading at the begging of line_buf.
 *
 *  \param line_buf the current line in file
 *  \param player_name user input player name
 *  \return 1 if name matches, 0 if not.
 */
int match_name(char *player_name, char *line_buf)
{
    if (line_buf[FIELD_SIZE - 1] != '\0')
    {
        return 0;
    }

    char name_in_line[FIELD_SIZE] = {'\0'};
    for (int i = 0; i < FIELD_SIZE; i++)
    {
        name_in_line[i] = line_buf[i];
    }
    name_in_line[FIELD_SIZE - 1] = '\0';
    if (strncmp(player_name, name_in_line, FIELD_SIZE) != 0)
    {
        return 0;
    }
    return 1;
}

/** Write score to existing record
 *  Starting from the current file pointer position - 21,
 *  10 character for name, 10 for score and 1 for 0x0A.
 *
 *  \param fp file pointer for the open file.
 *  \param player_name player's name to be written into file.
 *  \param score score to be written into file.
 *  \param file_pos position of file pointer to set
 *  \param offset position to start writing from
 *  \return 1 if write succeed, 0 if error is found.
 */
int write_score(FILE *fp, char *player_name, int score, int file_pos, int offset)
{
    char score_string[FIELD_SIZE + 1] = {'\0'};

    if (snprintf(score_string, 11, "%d", score) < 0)
    {
        return 0;
    }
    score_string[FIELD_SIZE] = '\n';

    if (fseek(fp, offset, file_pos) != -1)
    {
        if (fwrite(player_name, FIELD_SIZE, 1, fp) <= 0)
        {
            return 0;
        }
        if (fwrite(score_string, FIELD_SIZE + 1, 1, fp) <= 0)
        {
            return 0;
        }
        return 1;
    }
    return 0;
}

/** Reads the file line by line,
 *  if a match is found write the sum of score_to_add and current score
 *  by calling the write_score function.
 *
 *  \param fp file pointer for the open file.
 *  \param player_name player's name to be written into file.
 *  \param score score to be written into file.
 *  \return 1 if name is found and file has been written to without error,
 *  0 if not name was found. -1 if Error was found.
 */
int find_record(FILE *fp, char *player_name, int score_to_add)
{
    char *line_buf = NULL;
    size_t line_buf_size = 0;
    ssize_t line_size;

    if (fseek(fp, 0, SEEK_SET) == -1)
    {
        return -1;
    }

    line_size = getline(&line_buf, &line_buf_size, fp);
    while (line_size >= 0)
    {
        if (line_size != 21)
        {
            line_size = getline(&line_buf, &line_buf_size, fp);
            continue;
        }
        if (match_name(player_name, line_buf))
        {
            int current_score = get_score(line_buf);
            if (current_score == INT_MIN)
            {
                free(line_buf);
                return -1;
            }
            if (check_sum((long)current_score, (long)score_to_add))
            {
                if (write_score(fp, player_name, current_score + score_to_add, SEEK_CUR, -21) != 1)
                {
                    free(line_buf);
                    return -1;
                }
            }
            else
            {
                free(line_buf);
                return -1;
            }
            free(line_buf);
            return 1;
        }
        line_size = getline(&line_buf, &line_buf_size, fp);
    }
    free(line_buf);
    return 0;
}

/** Check if player_name is within acceptable length.
 *  Check if there are any white space characters.
 *
 *  \param player_name name of the player
 *  \return 1 if player name is valid, 0 if not.
 */
int validate_name(const char *player_name)
{
    ssize_t len = strlen(player_name);
    if (len > 10)
    {
        return 0;
    }
    for (int i = 0; i < len; i++)
    {
        if (isspace(player_name[i]))
        {
            return 0;
        }
    }
    return 1;
}

/** Adjust the score for player `player_name`, incrementing it by
 * `score_to_add`. The player's current score (if any) and new score
 * are stored in the scores file at `/var/lib/curdle/scores`.
 * The scores file is owned by user ID `uid`, and the process should
 * use that effective user ID when reading and writing the file.
 * If the score was changed successfully, the function returns 1; if
 * not, it returns 0, and sets `*message` to the address of
 * a string containing an error message. It is the caller's responsibility
 * to free `*message` after use.
 *
 *
 * \param uid user ID of the owner of the scores file.
 * \param player_name name of the player whose score should be incremented.
 * \param score_to_add amount by which to increment the score.
 * \param message address of a pointer in which an error message can be stored.
 * \return 1 if the score was successfully changed, 0 if not.
 */
int adjust_score(uid_t uid, const char *player_name, int score_to_add, char **message)
{
    /* if (seteuid(uid) != 0)
    {
        *message = handle_message("seteuid error\n");
        return 0;
    } */

    if (validate_name(player_name) != 1)
    {
        *message = handle_message("Player name invalid\n");
        return 0;
    }

    FILE *fp = fopen(FILENAME, "r+");
    if (!fp)
    {   
        *message = handle_message("File open error");
        return 0;
    }

    char name_to_search[FIELD_SIZE] = {'\0'};
    if (strncpy(name_to_search, player_name, FIELD_SIZE) == NULL)
    {   
        fclose(fp);
        *message = handle_message("String copy error\n");
        return 0;
    }

    name_to_search[FIELD_SIZE - 1] = '\0';

    int find_player = find_record(fp, name_to_search, score_to_add);
    if (find_player == -1)
    {   
        fclose(fp);
        *message = handle_message("Error found in record\n");
        return 0;
    }
    if (find_player == 0)
    {
        if (fseek(fp, 0, SEEK_END) == -1)
        {   
            fclose(fp);
            *message = handle_message("File seek error\n");
            return 0;
        }
        if (write_score(fp, name_to_search, score_to_add, SEEK_END, 0) != 1)
        {   
            fclose(fp);
            *message = handle_message("Error writing new record\n");
            return 0;
        }
    }

    /* if (seteuid(getuid()) != 0)
    {   
        fclose(fp);
        *message = handle_message("seteuid error\n");
        return 0;
    } */

    fclose(fp);
    return 1;
}

int main(int argc, char *argv[])
{
    /* if(setuid(1001) != 0){
        return 0;
    }
    if(setgid(1001) != 0){
        return 0;
    }
    if(seteuid(getuid()) != 0){
        return 0;
    }
    if(setegid(getgid()) != 0){
        return 0;
    } */
    uid_t uid = 1001;

    char **message = malloc(sizeof(char *) * 10);

    // long score_to_add = 1000;
    char player_name[60] = {'\0'};
    printf("Enter player name: \n");
    if (fgets(player_name, 60, stdin) == NULL)
    {
        fprintf(stderr, "player name error!\n");
        exit(EXIT_FAILURE);
    }
    /*
    char score_string[60] = {'\0'};
    printf("Enter player score: \n");
    if(fgets(score_string, 60, stdin) == NULL){
        fprintf(stderr, "player score error!\n");
        exit(EXIT_FAILURE);
    }
    score_string[60-1] = '\n';
    if(score_strtol(&score_to_add, score_string) == SCORE_INVALID){
        fprintf(stderr, "player score!\n");
        exit(EXIT_FAILURE);
    } */

    // const char* player_name = argv[1];
    int score_to_add = 1000;
    /* score_strtol(&score_to_add, argv[0]); */
    if (adjust_score(uid, (const char *)player_name, (int)score_to_add, &*message) == 1)
    {
        printf("Score write success\n");
        exit(EXIT_SUCCESS);
    }
    else
    {
        printf("%s\n", *message);
        exit(EXIT_FAILURE);
    }
    exit(EXIT_FAILURE);
}