#include <leveldb/c.h>
#include <stdio.h>

int main(int argc, char *argv[]) {
    leveldb_t *db;
    leveldb_options_t *options;
    leveldb_readoptions_t *roptions;
    leveldb_writeoptions_t *woptions;
    char *err = NULL;
    char *read;
    size_t read_len;

    /******************************************/
    /* OPEN */

    options = leveldb_options_create();
    leveldb_options_set_create_if_missing(options, 1);
    db = leveldb_open(options, "/tmp/libpaxos_leveldb", &err);

    if (err != NULL) {
      fprintf(stderr, "Open fail.\n");
      return(1);
    }

    /* reset error var */
    leveldb_free(err); err = NULL;

    /******************************************/
    /* WRITE */

    woptions = leveldb_writeoptions_create();
    leveldb_put(db, woptions, argv[1], 16, argv[2], 16, &err);

    if (err != NULL) {
      fprintf(stderr, "Write fail.\n");
      return(1);
    }

    leveldb_free(err); err = NULL;

    /******************************************/
    /* READ */

    roptions = leveldb_readoptions_create();
    read = leveldb_get(db, roptions, argv[1], 16, &read_len, &err);

    if (err != NULL) {
      fprintf(stderr, "Read fail.\n");
      return(1);
    }

    printf("key: %s\n", read);

    leveldb_free(err); err = NULL;

    /******************************************/
    /* DELETE */

    leveldb_delete(db, woptions, "x", 16, &err);

    if (err != NULL) {
      fprintf(stderr, "Delete fail.\n");
      return(1);
    }

    leveldb_free(err); err = NULL;

    /******************************************/
    /* CLOSE */

    leveldb_close(db);

    /******************************************/
    /* DESTROY */

    leveldb_destroy_db(options, "testdb", &err);

    if (err != NULL) {
      fprintf(stderr, "Destroy fail.\n");
      return(1);
    }

    leveldb_free(err); err = NULL;


    return(0);
}