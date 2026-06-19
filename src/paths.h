#ifndef QCC_PATHS_H
#define QCC_PATHS_H

void qcc_paths_init (const char * argv0);

const char * qcc_executable_path (void);
const char * qcc_basilisk_root (void);
const char * qcc_libdir (void);
const char * qcc_ast_std_dir (void);

char * qcc_path_join (const char * a, const char * b);

#endif
