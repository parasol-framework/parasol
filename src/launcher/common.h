
enum IntegrityLevel {
   INTEGRITY_LEVEL_SYSTEM,
   INTEGRITY_LEVEL_HIGH,
   INTEGRITY_LEVEL_MEDIUM,
   INTEGRITY_LEVEL_MEDIUM_LOW,
   INTEGRITY_LEVEL_LOW,
   INTEGRITY_LEVEL_BELOW_LOW,
   INTEGRITY_LEVEL_UNTRUSTED,
   INTEGRITY_LEVEL_UNKNOWN,
   INTEGRITY_LEVEL_LAST,
};

extern "C" {
IntegrityLevel get_integrity_level();
ERROR create_low_process(const char *ExePath, BYTE);
int get_exe(char *Buffer, int Size);
};
