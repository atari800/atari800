#define MIN_CARTTYPE 1
#define MAX_CARTTYPE 70
#define URL "https://github.com/atari800/atari800/blob/master/DOC/cart.txt"
#define MIN_SIZE 2048		/* Minimum cartridge size */

uint32_t CARTRIDGE_Checksum(const uint8_t *image, int nbytes);
long checklen(FILE *fp, int carttype);
void convert(char *romfile, char *cartfile, int32_t type);

typedef struct {
  uint8_t cart[4];
  uint8_t type[4];
  uint8_t csum[4];
  uint8_t zero[4];
} header_t;

typedef struct {
  int size;
  char *description;
} cart_t;

/* Valid cartrdige sizes by cartridge type */
static cart_t carts[MAX_CARTTYPE + 1] =
  {
    {     -1, "INVALID"},
    {   8192, "Standard 8 KB cartridge"},
    {  16384, "Standard 16 KB cartridge"},
    {  16384, "OSS two chip 16 KB cartridge (034M)"},
    {  32768, "Standard 32 KB 5200 cartridge"},
    {  32768, "DB 32 KB cartridge"},
    {  16384, "Two chip 16 KB 5200 cartridge"},
    {  40960, "Bounty Bob Strikes Back 40 KB 5200 cartridge"},
    {  65536, "64 KB Williams cartridge"},
    {  65536, "Express 64 KB cartridge"},
    {  65536, "Diamond 64 KB cartridge"},
    {  65536, "SpartaDOS X 64 KB cartridge"},
    {  32768, "XEGS 32 KB cartridge"},
    {  65536, "XEGS 64 KB cartridge (banks 0-7)"},
    { 131072, "XEGS 128 KB cartridge"},
    {  16384, "OSS one chip 16 KB cartridge"},
    {  16384, "One chip 16 KB 5200 cartridge"},
    { 131072, "Decoded Atrax 128 KB cartridge"},
    {  40960, "Bounty Bob Strikes Back 40 KB cartridge"},
    {   8192, "Standard 8 KB 5200 cartridge"},
    {   4096, "Standard 4 KB 5200 cartridge"},
    {   8192, "Right slot 8 KB cartridge"},
    {  32768, "32 KB Williams cartridge"},
    { 262144, "XEGS 256 KB cartridge"},
    { 524288, "XEGS 512 KB cartridge"},
    {1048576, "XEGS 1 MB cartridge"},
    {  16384, "MegaCart 16 KB cartridge"},
    {  32768, "MegaCart 32 KB cartridge"},
    {  65536, "MegaCart 64 KB cartridge"},
    { 131072, "MegaCart 128 KB cartridge"},
    { 262144, "MegaCart 256 KB cartridge"},
    { 524288, "MegaCart 512 KB cartridge"},
    {1048576, "MegaCart 1 MB cartridge"},
    {  32768, "Switchable XEGS 32 KB cartridge"},
    {  65536, "Switchable XEGS 64 KB cartridge"},
    { 131072, "Switchable XEGS 128 KB cartridge"},
    { 262144, "Switchable XEGS 256 KB cartridge"},
    { 524288, "Switchable XEGS 512 KB cartridge"},
    {1048576, "Switchable XEGS 1 MB cartridge"},
    {   8192, "Phoenix 8 KB cartridge"},
    {  16384, "Blizzard 16 KB cartridge"},
    { 131072, "Atarimax 128 KB Flash cartridge"},
    {1048576, "Atarimax 1 MB Flash cartridge"},
    { 131072, "SpartaDOS X 128 KB cartridge"},
    {   8192, "OSS 8 KB cartridge"},
    {  16384, "OSS two chip 16 KB cartridge (043M)"},
    {   4096, "Blizzard 4 KB cartridge"},
    {  32768, "AST 32 KB cartridge"},
    {  65536, "Atrax SDX 64 KB cartridge"},
    { 131072, "Atrax SDX 128 KB cartridge"},
    {  65536, "Turbosoft 64 KB cartridge"},
    { 131072, "Turbosoft 128 KB cartridge"},
    {  32768, "Ultracart 32 KB cartridge"},
    {   8192, "Low bank 8 KB cartridge"},
    { 131072, "SIC! 128 KB cartridge"},
    { 262144, "SIC! 256 KB cartridge"},
    { 524288, "SIC! 512 KB cartridge"},
    {   2048, "Standard 2 KB cartridge"},
    {   4096, "Standard 4 KB cartridge"},
    {   4096, "Right slot 4 KB cartridge"},
    {  32768, "Blizzard 32 KB cartridge"},
    {2097152, "MegaMax 2 MB cartridge"},
    { 131072, "The!Cart 128 MB cartridge"},
    {4194304, "Flash MegaCart 4 MB cartridge"},
    {2097152, "MegaCart 2 MB cartridge"},
    {  32768, "The!Cart 32 MB cartridge"},
    {  65536, "The!Cart 64 MB cartridge"},
    {  65536, "XEGS 64 KB cartridge (banks 8-15)"},
    { 131072, "Atrax 128 KB cartridge"},
    {  32768, "aDawliah 32 KB cartridge"},
    {  65536, "aDawliah 64 KB cartridge"},
  };		       
