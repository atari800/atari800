int SaveAtariState( char *filename, const char *mode, UBYTE SaveVerbose );
int ReadAtariState( char *filename, const char *mode );
extern void SaveUBYTE( UBYTE *data, int num );
extern void SaveUWORD( UWORD *data, int num );
extern void SaveINT( int *data, int num );
extern void ReadUBYTE( UBYTE *data, int num );
extern void ReadUWORD( UWORD *data, int num );
extern void ReadINT( int *data, int num );
