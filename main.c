#include <efi.h>
#include <efilib.h>

int abs(int a)
{
  if (a < 0)
  {
    return -a;
  }
  else
  {
    return a;
  }
}

//Graphics Output Protocol code based on the code found at https://wiki.osdev.org/GOP

int setup_gop(EFI_SYSTEM_TABLE *SystemTable, EFI_GRAPHICS_OUTPUT_PROTOCOL **inputGop, int desired_width, int desired_height)
{
  EFI_STATUS status;
  EFI_GUID gopGuid = EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID;

  status = uefi_call_wrapper(BS->LocateProtocol, 3, &gopGuid, NULL, (void **)inputGop);
  if (EFI_ERROR(status))
  {
    Print(L"Unable to locate GOP\n");
    return 0;
  }
  EFI_GRAPHICS_OUTPUT_PROTOCOL *Gop = *inputGop;

  EFI_GRAPHICS_OUTPUT_MODE_INFORMATION *info;
  UINTN SizeOfInfo, numModes;
  status = uefi_call_wrapper(Gop->QueryMode, 4, Gop, Gop->Mode == NULL ? 0 : Gop->Mode->Mode, &SizeOfInfo, &info);
  // this is needed to get the current video mode
  if (status == EFI_NOT_STARTED)
    status = uefi_call_wrapper(Gop->SetMode, 2, Gop, 0);
  if (EFI_ERROR(status))
  {
    Print(L"Unable to get native mode\n");
    return 0;
  }
  else
  {
    numModes = Gop->Mode->MaxMode;
  }

  int tmp_current_mode = 0;
  int tmp_current_width = -9999;
  int tmp_current_height = -9999;
  for (int i = 0; i < numModes; i++)
  {
    status = uefi_call_wrapper(Gop->QueryMode, 4, Gop, i, &SizeOfInfo, &info);

    if (info->HorizontalResolution >= desired_width && info->VerticalResolution >= desired_height)
    {
      // wartosc okresla dopasowanie trybu do zadanych parametrow (im mniejsza tym lepiej)
      int current_mode_fit = abs(tmp_current_width - desired_width) + abs(tmp_current_height - desired_height);
      int this_mode_fit = abs(info->HorizontalResolution - desired_width) + abs(info->VerticalResolution - desired_height);
      if (this_mode_fit < current_mode_fit)
      {
        tmp_current_mode = i;
        tmp_current_width = info->HorizontalResolution;
        tmp_current_height = info->VerticalResolution;
        Print(L"Wybrano: %d\n", tmp_current_mode);
      }
    }
  }

  status = uefi_call_wrapper(Gop->SetMode, 2, Gop, tmp_current_mode);
  if (EFI_ERROR(status))
  {
    Print(L"Unable to set mode %03d\n", 0);
    return 0;
  }

  return 1;
}

struct GraphicBuffer
{
  EFI_GRAPHICS_OUTPUT_BLT_PIXEL *pixelArray;
  int width;
  int height;
};
void createGraphicBuffer(EFI_SYSTEM_TABLE *SystemTable, struct GraphicBuffer *buf, int width, int height)
{
  buf->height = height;
  buf->width = width;
  EFI_STATUS status = uefi_call_wrapper(SystemTable->BootServices->AllocatePool, 3, EfiBootServicesData, sizeof(EFI_GRAPHICS_OUTPUT_BLT_PIXEL) * height * width, (void **)(&(buf->pixelArray)));
  if (status == EFI_SUCCESS)
  {
    Print(L"SUCCESS\n");
  }
  else if (status == EFI_INVALID_PARAMETER)
  {
    Print(L"INVALID\n");
  }
}

//File access code based on the code found at https://wiki.osdev.org/Loading_files_under_UEFI

EFI_FILE_HANDLE GetVolume(EFI_HANDLE image)
{
  EFI_LOADED_IMAGE *loaded_image = NULL;            
  EFI_GUID lipGuid = EFI_LOADED_IMAGE_PROTOCOL_GUID;     
  EFI_FILE_IO_INTERFACE *IOVolume;                       
  EFI_GUID fsGuid = EFI_SIMPLE_FILE_SYSTEM_PROTOCOL_GUID; 
  EFI_FILE_HANDLE Volume;                             


  uefi_call_wrapper(BS->HandleProtocol, 3, image, &lipGuid, (void **)&loaded_image);
  uefi_call_wrapper(BS->HandleProtocol, 3, loaded_image->DeviceHandle, &fsGuid, (VOID *)&IOVolume);
  uefi_call_wrapper(IOVolume->OpenVolume, 2, IOVolume, &Volume);
  return Volume;
}
UINT64 FileSize(EFI_FILE_HANDLE FileHandle)
{
  UINT64 ret;
  EFI_FILE_INFO *FileInfo;
  FileInfo = LibFileInfo(FileHandle);
  ret = FileInfo->FileSize;
  FreePool(FileInfo);
  return ret;
}

void loadBitmap(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable, wchar_t *path, struct GraphicBuffer *bmpOut)
{
  EFI_FILE_HANDLE Volume = GetVolume(ImageHandle);
  wchar_t *FilePath = path;
  EFI_FILE_HANDLE FileHandle;

  /* open the file */
  uefi_call_wrapper(Volume->Open, 5, Volume, &FileHandle, FilePath, EFI_FILE_MODE_READ, EFI_FILE_READ_ONLY | EFI_FILE_HIDDEN | EFI_FILE_SYSTEM);
  UINT64 ReadSize = FileSize(FileHandle);
  UINT8 *Buffer = AllocatePool(ReadSize);

  uefi_call_wrapper(FileHandle->Read, 3, FileHandle, &ReadSize, Buffer);

  int tmpWidth = *(uint32_t *)(Buffer + 18);
  int tmpHeight = *(uint32_t *)(Buffer + 22);

  createGraphicBuffer(SystemTable, bmpOut, tmpWidth, tmpHeight);

  uint32_t dataOffset = *(uint32_t *)(Buffer + 10);
  for (int y = 0; y < tmpHeight; y++)
  {
    for (int x = 0; x < tmpWidth; x++)
    {
      int reverseY = tmpHeight - y - 1;
      bmpOut->pixelArray[x + reverseY * tmpWidth].Blue = Buffer[4 * (x + y * tmpWidth) + dataOffset];
      bmpOut->pixelArray[x + reverseY * tmpWidth].Green = Buffer[4 * (x + y * tmpWidth) + dataOffset + 1];
      bmpOut->pixelArray[x + reverseY * tmpWidth].Red = Buffer[4 * (x + y * tmpWidth) + dataOffset + 2];
      bmpOut->pixelArray[x + reverseY * tmpWidth].Reserved = Buffer[4 * (x + y * tmpWidth) + dataOffset + 3];
    }
  }
  FreePool(Buffer);
};

void bltGraphicBuffer(struct GraphicBuffer *srcGraphic, struct GraphicBuffer *destGraphic, int destX, int destY)
{
  int w_begin = 0;
  int w_end = srcGraphic->width;
  int h_begin = 0;
  int h_end = srcGraphic->height;
  if (destX < 0)
  {
    w_begin = -destX;
  }
  if (destY < 0)
  {
    h_begin = -destY;
  }

  if (destX + srcGraphic->width > destGraphic->width)
  {
    w_end = destGraphic->width - destX;
  }
  if (destY + srcGraphic->height > destGraphic->height)
  {
    h_end = destGraphic->height - destY;
  }

  if (w_begin < 0)
  {
    return;
  }
  if (h_begin < 0)
  {
    return;
  }

  if (w_end > srcGraphic->width)
  {
    return;
  }
  if (h_end > srcGraphic->height)
  {
    return;
  }

  for (int w = w_begin; w < w_end; w++)
  {
    for (int h = h_begin; h < h_end; h++)
    {
      destGraphic->pixelArray[(destX + w) + (h + destY) * destGraphic->width] = srcGraphic->pixelArray[w + h * srcGraphic->width];
    }
  }
};

void bltGraphicBufferTransparentPixel(struct GraphicBuffer *srcGraphic, struct GraphicBuffer *destGraphic, int destX, int destY, EFI_GRAPHICS_OUTPUT_BLT_PIXEL transparent_pixel)
{
  int w_begin = 0;
  int w_end = srcGraphic->width;
  int h_begin = 0;
  int h_end = srcGraphic->height;
  if (destX < 0)
  {
    w_begin = -destX;
  }
  if (destY < 0)
  {
    h_begin = -destY;
  }

  if (destX + srcGraphic->width > destGraphic->width)
  {
    w_end = destGraphic->width - destX;
  }
  if (destY + srcGraphic->height > destGraphic->height)
  {
    h_end = destGraphic->height - destY;
  }

  if (w_begin < 0)
  {
    return;
  }
  if (h_begin < 0)
  {
    return;
  }

  if (w_end > srcGraphic->width)
  {
    return;
  }
  if (h_end > srcGraphic->height)
  {
    return;
  }

  for (int w = w_begin; w < w_end; w++)
  {
    for (int h = h_begin; h < h_end; h++)
    {
      EFI_GRAPHICS_OUTPUT_BLT_PIXEL pix = srcGraphic->pixelArray[w + h * srcGraphic->width];
      if(pix.Blue == transparent_pixel.Blue && pix.Green == transparent_pixel.Green && pix.Red == transparent_pixel.Red)
      {
        continue;
      }
      destGraphic->pixelArray[(destX + w) + (h + destY) * destGraphic->width] = srcGraphic->pixelArray[w + h * srcGraphic->width];
    }
  }
};
// BLOCK GAME SECTION BEGIN

#define BLOCK_GAME_WIDTH 10
#define BLOCK_GAME_HEIGHT 20
#define BLOCK_MAX_SIDE_LEN 4

struct block_game_falling_block
{
  int x_pos_block;
  int y_pos_block;
  int side_len;
  char block[BLOCK_MAX_SIDE_LEN][BLOCK_MAX_SIDE_LEN];
};
void emptyBlockGameFallingBlock(struct block_game_falling_block *block)
{
  block->side_len = 0;
  block->x_pos_block = 0;
  block->y_pos_block = 0;
  for (int y = 0; y < BLOCK_MAX_SIDE_LEN; y++)
  {
    for (int x = 0; x < BLOCK_MAX_SIDE_LEN; x++)
    {
      block->block[x][y] = 0;
    }
  }
}
enum falling_block_type
{
  L_BLOCK,
  J_BLOCK,
  O_BLOCK,
  S_BLOCK,
  Z_BLOCK,
  T_BLOCK,
  I_BLOCK
};
void createBlockGameFallingBlock(struct block_game_falling_block *block, enum falling_block_type type)
{
  emptyBlockGameFallingBlock(block);
  char *tmp_block_pointer = NULL;
  char tmp_block_outside[BLOCK_MAX_SIDE_LEN * BLOCK_MAX_SIDE_LEN];
  if (type == L_BLOCK)
  {
    block->side_len = 3;
    char tmp_block[BLOCK_MAX_SIDE_LEN * BLOCK_MAX_SIDE_LEN] =
        {
            0, 1, 0, 0,
            0, 1, 0, 0,
            0, 1, 1, 0,
            0, 0, 0, 0};
    for(int i = 0; i<BLOCK_MAX_SIDE_LEN*BLOCK_MAX_SIDE_LEN; i++)
    {
      tmp_block_outside[i] = tmp_block[i];
    }
    tmp_block_pointer = tmp_block_outside;
  }
  else if (type == J_BLOCK)
  {
    block->side_len = 3;
    char tmp_block[BLOCK_MAX_SIDE_LEN * BLOCK_MAX_SIDE_LEN] =
        {
            0, 2, 0, 0,
            0, 2, 0, 0,
            2, 2, 0, 0,
            0, 0, 0, 0};
    for(int i = 0; i<BLOCK_MAX_SIDE_LEN*BLOCK_MAX_SIDE_LEN; i++)
    {
      tmp_block_outside[i] = tmp_block[i];
    }
    tmp_block_pointer = tmp_block_outside;
  }
  else if (type == O_BLOCK)
  {
    block->side_len = 2;
    char tmp_block[BLOCK_MAX_SIDE_LEN * BLOCK_MAX_SIDE_LEN] =
        {
            3, 3, 0, 0,
            3, 3, 0, 0,
            0, 0, 0, 0,
            0, 0, 0, 0};
    for(int i = 0; i<BLOCK_MAX_SIDE_LEN*BLOCK_MAX_SIDE_LEN; i++)
    {
      tmp_block_outside[i] = tmp_block[i];
    }
    tmp_block_pointer = tmp_block_outside;
  }
  else if (type == S_BLOCK)
  {
    block->side_len = 3;
    char tmp_block[BLOCK_MAX_SIDE_LEN * BLOCK_MAX_SIDE_LEN] =
        {
            0, 0, 0, 0,
            0, 4, 4, 0,
            4, 4, 0, 0,
            0, 0, 0, 0};
    for(int i = 0; i<BLOCK_MAX_SIDE_LEN*BLOCK_MAX_SIDE_LEN; i++)
    {
      tmp_block_outside[i] = tmp_block[i];
    }
    tmp_block_pointer = tmp_block_outside;
  }
  else if (type == Z_BLOCK)
  {
    block->side_len = 3;
    char tmp_block[BLOCK_MAX_SIDE_LEN * BLOCK_MAX_SIDE_LEN] =
        {
            0, 0, 0, 0,
            5, 5, 0, 0,
            0, 5, 5, 0,
            0, 0, 0, 0};
    for(int i = 0; i<BLOCK_MAX_SIDE_LEN*BLOCK_MAX_SIDE_LEN; i++)
    {
      tmp_block_outside[i] = tmp_block[i];
    }
    tmp_block_pointer = tmp_block_outside;
  }
  else if (type == T_BLOCK)
  {
    block->side_len = 3;
    char tmp_block[BLOCK_MAX_SIDE_LEN * BLOCK_MAX_SIDE_LEN] =
        {
            0, 0, 0, 0,
            6, 6, 6, 0,
            0, 6, 0, 0,
            0, 0, 0, 0};
    for(int i = 0; i<BLOCK_MAX_SIDE_LEN*BLOCK_MAX_SIDE_LEN; i++)
    {
      tmp_block_outside[i] = tmp_block[i];
    }
    tmp_block_pointer = tmp_block_outside;
  }
  else if (type == I_BLOCK)
  {
    block->side_len = 4;
    char tmp_block[BLOCK_MAX_SIDE_LEN * BLOCK_MAX_SIDE_LEN] =
        {
            0, 0, 7, 0,
            0, 0, 7, 0,
            0, 0, 7, 0,
            0, 0, 7, 0};
    for(int i = 0; i<BLOCK_MAX_SIDE_LEN*BLOCK_MAX_SIDE_LEN; i++)
    {
      tmp_block_outside[i] = tmp_block[i];
    }
    tmp_block_pointer = tmp_block_outside;
  }
  else
  {
    return;
  }
  for (int y = 0; y < BLOCK_MAX_SIDE_LEN; y++)
  {
    for (int x = 0; x < BLOCK_MAX_SIDE_LEN; x++)
    {
      block->block[x][y] = tmp_block_pointer[x + y * BLOCK_MAX_SIDE_LEN];
    }
  }
}

void rotateBlockGameFallingBlockClockwiseSimple(struct block_game_falling_block *block)
{
  struct block_game_falling_block tmp_block = *block;
  for (int y = 0; y < tmp_block.side_len; y++)
  {
    for (int x = 0; x < tmp_block.side_len; x++)
    {
      block->block[tmp_block.side_len - y - 1][x] = tmp_block.block[x][y];
    }
  }
}

void rotateBlockGameFallingBlockCounterClockwiseSimple(struct block_game_falling_block *block)
{
  struct block_game_falling_block tmp_block = *block;
  for (int y = 0; y < tmp_block.side_len; y++)
  {
    for (int x = 0; x < tmp_block.side_len; x++)
    {
      block->block[x][y] = tmp_block.block[tmp_block.side_len - y - 1][x];
    }
  }
}

struct block_game_board
{
  int width;
  int height;
  char board[BLOCK_GAME_WIDTH][BLOCK_GAME_HEIGHT];
};

void createBlockGameBoard(struct block_game_board *board)
{
  board->height = BLOCK_GAME_HEIGHT;
  board->width = BLOCK_GAME_WIDTH;
  for (int y = 0; y < board->height; y++)
  {
    for (int x = 0; x < board->width; x++)
    {
      board->board[x][y] = 0;
    }
  }
}

struct block_graphics_data
{
  struct GraphicBuffer block[7];
};

void createBlockGraphicsData(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable, struct block_graphics_data *data)
{
  Print(L"LOADING BLOCKS\n");
  loadBitmap(ImageHandle, SystemTable, L"IMG\\BLOCK1.bmp", &(data->block[0]));
  loadBitmap(ImageHandle, SystemTable, L"IMG\\BLOCK2.bmp", &(data->block[1]));
  loadBitmap(ImageHandle, SystemTable, L"IMG\\BLOCK3.bmp", &(data->block[2]));
  loadBitmap(ImageHandle, SystemTable, L"IMG\\BLOCK4.bmp", &(data->block[3]));
  loadBitmap(ImageHandle, SystemTable, L"IMG\\BLOCK5.bmp", &(data->block[4]));
  loadBitmap(ImageHandle, SystemTable, L"IMG\\BLOCK6.bmp", &(data->block[5]));
  loadBitmap(ImageHandle, SystemTable, L"IMG\\BLOCK7.bmp", &(data->block[6]));
  Print(L"ENDED LOADING BLOCKS\n");
}

struct font_graphics_data
{
  struct GraphicBuffer number_font[10];
};

void createFontGraphicsData(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable, struct font_graphics_data *data)
{
  Print(L"LOADING FONT\n");
  loadBitmap(ImageHandle, SystemTable, L"IMG\\FONT0.bmp", &(data->number_font[0]));
  loadBitmap(ImageHandle, SystemTable, L"IMG\\FONT1.bmp", &(data->number_font[1]));
  loadBitmap(ImageHandle, SystemTable, L"IMG\\FONT2.bmp", &(data->number_font[2]));
  loadBitmap(ImageHandle, SystemTable, L"IMG\\FONT3.bmp", &(data->number_font[3]));
  loadBitmap(ImageHandle, SystemTable, L"IMG\\FONT4.bmp", &(data->number_font[4]));
  loadBitmap(ImageHandle, SystemTable, L"IMG\\FONT5.bmp", &(data->number_font[5]));
  loadBitmap(ImageHandle, SystemTable, L"IMG\\FONT6.bmp", &(data->number_font[6]));
  loadBitmap(ImageHandle, SystemTable, L"IMG\\FONT7.bmp", &(data->number_font[7]));
  loadBitmap(ImageHandle, SystemTable, L"IMG\\FONT8.bmp", &(data->number_font[8]));
  loadBitmap(ImageHandle, SystemTable, L"IMG\\FONT9.bmp", &(data->number_font[9]));
  Print(L"ENDED LOADING FONT\n");
}

struct block_game_graphics_data
{
  struct GraphicBuffer screen_background;
  struct GraphicBuffer background;
  struct GraphicBuffer default_block;
  struct GraphicBuffer score_background;
  struct GraphicBuffer help;
  struct GraphicBuffer game_over;
  struct block_graphics_data blocks;
  struct font_graphics_data font;
};
void createBlockGameGraphicsData(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable, struct block_game_graphics_data *data)
{
  Print(L"LOADING BITMAPS\n");
  loadBitmap(ImageHandle, SystemTable, L"IMG\\BACKGROUND.bmp", &(data->screen_background));
  loadBitmap(ImageHandle, SystemTable, L"IMG\\SCORE_BACKGROUND.bmp", &(data->score_background));
  loadBitmap(ImageHandle, SystemTable, L"IMG\\BACK.bmp", &(data->background));
  loadBitmap(ImageHandle, SystemTable, L"IMG\\BLOCKDEF.bmp", &(data->default_block));
  loadBitmap(ImageHandle, SystemTable, L"IMG\\HELP.bmp", &(data->help));
  loadBitmap(ImageHandle, SystemTable, L"IMG\\GAME_OVER.bmp", &(data->game_over));
  createBlockGraphicsData(ImageHandle,SystemTable, &data->blocks);
  createFontGraphicsData(ImageHandle,SystemTable,&data->font);
  Print(L"ENDED LOADING BITMAPS\n");
}


// based on https://en.wikipedia.org/wiki/Linear_congruential_generator
struct random_number_generator
{
  uint64_t current_value;
  uint64_t modulus;
  uint64_t multiplier;
  uint64_t increment;
};

void createRandomNumberGenerator(struct random_number_generator *generator, uint64_t seed)
{
  generator->modulus = ((uint64_t)1) << ((uint64_t)31);
  generator->multiplier = (uint64_t)1103515245;
  generator->increment = (uint64_t)12345;
  generator->current_value = seed;
}

uint64_t getNextRandomValue(struct random_number_generator *generator)
{
  uint64_t next_value = (generator->multiplier * generator->current_value + generator->increment) % generator->modulus;
  generator->current_value = next_value;
  return next_value;
}
enum falling_block_type getRandomBlockType(struct random_number_generator *generator)
{
  enum falling_block_type type_arr[] = {
      L_BLOCK,
      J_BLOCK,
      O_BLOCK,
      S_BLOCK,
      Z_BLOCK,
      T_BLOCK,
      I_BLOCK};
  int chosen_type = getNextRandomValue(generator) % 7;
  return type_arr[chosen_type];
}

struct block_game_game
{
  EFI_HANDLE ImageHandle;
  EFI_SYSTEM_TABLE *SystemTable;
  int is_game_finished;
  int current_score;
  int level;
  struct block_game_falling_block falling_block;
  struct block_game_board board;
  struct block_game_graphics_data graphics_data;
  struct random_number_generator generator;
};

void blockGameCenterFallingBlock(struct block_game_game *block_game)
{
  block_game->falling_block.x_pos_block = block_game->board.width / 2 - (block_game->falling_block.side_len - block_game->falling_block.side_len / 2);
}

void createBlockGameGame(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable, struct block_game_game *block_game, uint64_t rand_seed)
{
  block_game->ImageHandle = ImageHandle;
  block_game->SystemTable = SystemTable;
  block_game->is_game_finished = 0;
  block_game->current_score = 0;
  block_game->level = 0;
  createRandomNumberGenerator(&block_game->generator, rand_seed);
  createBlockGameBoard(&block_game->board);

  createBlockGameFallingBlock(&block_game->falling_block, getRandomBlockType(&block_game->generator));
  blockGameCenterFallingBlock(block_game);

  createBlockGameGraphicsData(ImageHandle, SystemTable, &block_game->graphics_data);
}

void resetBlockGameGame(struct block_game_game *block_game)
{
  block_game->is_game_finished = 0;
  block_game->current_score = 0;
  block_game->level = 0;
  createBlockGameBoard(&block_game->board);

  createBlockGameFallingBlock(&block_game->falling_block, getRandomBlockType(&block_game->generator));
  blockGameCenterFallingBlock(block_game);
}

void drawBlockGameBoard(struct block_game_game *block_game, struct GraphicBuffer *screen, int posX, int posY)
{
  bltGraphicBuffer(&block_game->graphics_data.background, screen, posX, posY);
  for (int y = 0; y < block_game->board.height; y++)
  {
    for (int x = 0; x < block_game->board.width; x++)
    {
      if (block_game->board.board[x][y] != 0)
      {
        struct GraphicBuffer *current_block_bmp = &block_game->graphics_data.default_block;
        int block_value = block_game->board.board[x][y];
        if(block_value > 0 && block_value < (sizeof(block_game->graphics_data.blocks)/sizeof(struct GraphicBuffer) + 1))
        {
          current_block_bmp = &block_game->graphics_data.blocks.block[block_value-1];
        }
        int current_x_pos = posX + x * current_block_bmp->width;
        int current_y_pos = posY + y * current_block_bmp->height;
        bltGraphicBuffer(current_block_bmp, screen, current_x_pos, current_y_pos);
      }
    }
  }
}

int isBoardSpaceEmpty(struct block_game_board *board, int x, int y)
{
  if (x < 0 || x >= board->width)
  {
    return 0;
  }
  if (y < 0 || y >= board->height)
  {
    return 0;
  }
  if (board->board[x][y] == 0)
  {
    return 1;
  }
  else
  {
    return 0;
  }
}

int isFallingBlockValidPosition(struct block_game_falling_block *falling_block, struct block_game_board *board)
{
  for (int y = 0; y < falling_block->side_len; y++)
  {
    for (int x = 0; x < falling_block->side_len; x++)
    {
      if (falling_block->block[x][y] != 0)
      {
        int board_x = x + falling_block->x_pos_block;
        int board_y = y + falling_block->y_pos_block;
        if (!isBoardSpaceEmpty(board, board_x, board_y))
        {
          return 0;
        }
      }
    }
  }
  return 1;
}

void copyFallingBlockContentsToBoard(struct block_game_falling_block *falling_block, struct block_game_board *board)
{
  for (int y = 0; y < falling_block->side_len; y++)
  {
    for (int x = 0; x < falling_block->side_len; x++)
    {
      if (falling_block->block[x][y] != 0)
      {
        int board_x = x + falling_block->x_pos_block;
        int board_y = y + falling_block->y_pos_block;
        if (board_x < 0 || board_x >= board->width)
        {
          continue;
        }
        if (board_y < 0 || board_y >= board->height)
        {
          continue;
        }
        board->board[board_x][board_y] = falling_block->block[x][y];
      }
    }
  }
}

void drawBlockGameFallingBlock(struct block_game_game *block_game, struct GraphicBuffer *screen, int posX, int posY)
{
  for (int y = 0; y < block_game->falling_block.side_len; y++)
  {
    for (int x = 0; x < block_game->falling_block.side_len; x++)
    {
      if (block_game->falling_block.block[x][y] != 0)
      {
        struct GraphicBuffer *current_block_bmp = &block_game->graphics_data.default_block;
        int block_value = block_game->falling_block.block[x][y];
        if(block_value > 0 && block_value < (sizeof(block_game->graphics_data.blocks)/sizeof(struct GraphicBuffer) + 1))
        {
          current_block_bmp = &block_game->graphics_data.blocks.block[block_value-1];
        }
        int current_x_pos = posX + (x + block_game->falling_block.x_pos_block) * current_block_bmp->width;
        int current_y_pos = posY + (y + block_game->falling_block.y_pos_block) * current_block_bmp->height;
        bltGraphicBuffer(current_block_bmp, screen, current_x_pos, current_y_pos);
      }
    }
  }
}

void drawBlockGameGame(struct block_game_game *block_game, struct GraphicBuffer *screen, int posX, int posY)
{
  drawBlockGameBoard(block_game, screen, posX, posY);
  drawBlockGameFallingBlock(block_game, screen, posX, posY);
  if(block_game->is_game_finished)
  {
    int x_offset = ((BLOCK_GAME_WIDTH * block_game->graphics_data.default_block.width)/2) - (block_game->graphics_data.game_over.width/2);
    int y_offset = ((BLOCK_GAME_HEIGHT * block_game->graphics_data.default_block.height)/2) - (block_game->graphics_data.game_over.height/2);
    bltGraphicBuffer(&block_game->graphics_data.game_over, screen, posX + x_offset, posY + y_offset);
  }
};

void blockGameMovePieceLeft(struct block_game_game *block_game)
{
  struct block_game_falling_block tmp_block = block_game->falling_block;
  tmp_block.x_pos_block--;
  if (isFallingBlockValidPosition(&tmp_block, &block_game->board))
  {
    block_game->falling_block = tmp_block;
    return;
  }
  else
  {
    return;
  }
}

void drawNumber(struct block_game_game *block_game, struct GraphicBuffer *screen, int posX, int posY, unsigned int number)
{
  int relative_x_pos = 0;
  int relative_x_pos_prepass = 0;
  EFI_GRAPHICS_OUTPUT_BLT_PIXEL transparent_color;
  transparent_color.Blue = 0;
  transparent_color.Green = 0;
  transparent_color.Red = 0;
  if(number == 0)
  {
    bltGraphicBufferTransparentPixel(&block_game->graphics_data.font.number_font[0],screen,posX,posY, transparent_color);
  }
  int tmp_number = number;
  while(tmp_number != 0)
  {
    relative_x_pos_prepass += block_game->graphics_data.font.number_font[tmp_number%10].width;
    tmp_number /= 10;
  }
  relative_x_pos = relative_x_pos_prepass;
  while(number != 0)
  {
    relative_x_pos -= block_game->graphics_data.font.number_font[number%10].width;
    bltGraphicBufferTransparentPixel(&block_game->graphics_data.font.number_font[number%10],screen,posX + relative_x_pos,posY, transparent_color);
    number /= 10;
  }

}

void drawBlockGameInfoTable(struct block_game_game *block_game, struct GraphicBuffer *screen, int posX, int posY)
{
  bltGraphicBuffer(&block_game->graphics_data.score_background,screen,posX,posY);
  drawNumber(block_game,screen,posX + 121,posY + 15,block_game->level);
  drawNumber(block_game,screen,posX+121,posY+50,block_game->current_score);
}

void blockGameMovePieceRight(struct block_game_game *block_game)
{
  struct block_game_falling_block tmp_block = block_game->falling_block;
  tmp_block.x_pos_block++;
  if (isFallingBlockValidPosition(&tmp_block, &block_game->board))
  {
    block_game->falling_block = tmp_block;
    return;
  }
  else
  {
    return;
  }
}

void blockGameRotatePieceClockwise(struct block_game_game *block_game)
{
  struct block_game_falling_block tmp_block = block_game->falling_block;
  rotateBlockGameFallingBlockClockwiseSimple(&tmp_block);
  if (isFallingBlockValidPosition(&tmp_block, &block_game->board))
  {
    block_game->falling_block = tmp_block;
    return;
  }
  else
  {
    return;
  }
}

void blockGameRotatePieceCounterClockwise(struct block_game_game *block_game)
{
  struct block_game_falling_block tmp_block = block_game->falling_block;
  rotateBlockGameFallingBlockCounterClockwiseSimple(&tmp_block);
  if (isFallingBlockValidPosition(&tmp_block, &block_game->board))
  {
    block_game->falling_block = tmp_block;
    return;
  }
  else
  {
    return;
  }
}

// return number of lines cleared
int blockGameClearFullLines(struct block_game_game *block_game)
{
  int number_of_lines_cleared = 0;
  for (int y = 0; y < block_game->board.height; y++)
  {
    int lineFull = 1;
    for (int x = 0; x < block_game->board.width; x++)
    {
      if (block_game->board.board[x][y] == 0)
      {
        lineFull = 0;
        break;
      }
    }

    if (!lineFull)
    {
      continue;
    }
    number_of_lines_cleared++;
    for (int y_2 = y; y_2 >= 1; y_2--)
    {
      for (int x_2 = 0; x_2 < block_game->board.width; x_2++)
      {
        block_game->board.board[x_2][y_2] = block_game->board.board[x_2][y_2 - 1];
      }
    }
    for (int x_3 = 0; x_3 < block_game->board.width; x_3++)
    {
      block_game->board.board[x_3][0] = 0;
    }
  }
  return number_of_lines_cleared;
}

// zwraca 0 jezeli stworzono nowy
int blockGameMovePieceDown(struct block_game_game *block_game)
{
  struct block_game_falling_block tmp_block = block_game->falling_block;
  tmp_block.y_pos_block++;
  if (isFallingBlockValidPosition(&tmp_block, &block_game->board))
  {
    block_game->falling_block = tmp_block;
    return 1;
  }
  else
  {
    copyFallingBlockContentsToBoard(&block_game->falling_block, &block_game->board);
    block_game->current_score += blockGameClearFullLines(block_game) * 1000;
    createBlockGameFallingBlock(&block_game->falling_block, getRandomBlockType(&block_game->generator));
    blockGameCenterFallingBlock(block_game);
    if (!isFallingBlockValidPosition(&block_game->falling_block, &block_game->board))
    {
      block_game->is_game_finished = 1;
    }
    return 0;
  }
}

void blockGameHardDrop(struct block_game_game *block_game)
{
  int spaces_moved_down = 0;
  while (blockGameMovePieceDown(block_game))
    spaces_moved_down++;

  block_game->current_score += spaces_moved_down * 2;
}

// BLOCK GAME SECTION END

EFI_STATUS
EFIAPI
efi_main(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable)
{

  InitializeLib(ImageHandle, SystemTable);
  int desiredWidth = 1280;
  int desiredHeight = 720;

  EFI_GRAPHICS_OUTPUT_PROTOCOL *Gop;
  setup_gop(SystemTable, &Gop, desiredWidth, desiredHeight);

  struct GraphicBuffer buf;
  createGraphicBuffer(SystemTable, &buf, desiredWidth, desiredHeight);

  EFI_TIME current_time;
  uefi_call_wrapper(SystemTable->RuntimeServices->GetTime, 2, &current_time, NULL);
  uint64_t time_random_seed = current_time.Second + current_time.Minute*60 + current_time.Hour*60*60;
  //ustawienie losowego seeda do generatora losowego na podstawie czasu
  struct block_game_game block_game;
  createBlockGameGame(ImageHandle, SystemTable, &block_game, time_random_seed);



  EFI_EVENT timer;
  uefi_call_wrapper(SystemTable->BootServices->CreateEvent, 5, EVT_TIMER, TPL_APPLICATION, NULL, NULL, &timer);

  int desired_ticks_per_second = 30;
  uefi_call_wrapper(SystemTable->BootServices->SetTimer, 3, timer, TimerPeriodic, 10000000 / desired_ticks_per_second);

  uefi_call_wrapper(SystemTable->ConIn->Reset, 2, SystemTable->ConIn, FALSE);

  EFI_EVENT table[] = {timer};
  int number_of_events = sizeof(table) / sizeof(EFI_EVENT);

  uint64_t tick_count = 0;
  bltGraphicBuffer(&block_game.graphics_data.screen_background,&buf,0,0);
  while (1)
  {
    tick_count++;
    UINTN index;
    uefi_call_wrapper(SystemTable->BootServices->WaitForEvent, 3, number_of_events, table, &index);

    EFI_INPUT_KEY key;
    while (uefi_call_wrapper(SystemTable->ConIn->ReadKeyStroke, 2, SystemTable->ConIn, &key) != EFI_NOT_READY)
    {
      if (!block_game.is_game_finished)
      {
        if (key.UnicodeChar == L'z')
        {
          blockGameRotatePieceCounterClockwise(&block_game);
        }
        if (key.UnicodeChar == L'x')
        {
          blockGameRotatePieceClockwise(&block_game);
        }
        // UEFI SCAN CODES
        UINT16 LEFT_ARROW_SCAN_CODE = 0x04;
        UINT16 RIGHT_ARROW_SCAN_CODE = 0x03;
        UINT16 DOWN_ARROW_SCAN_CODE = 0x02;
        if (key.ScanCode == LEFT_ARROW_SCAN_CODE)
        {
          blockGameMovePieceLeft(&block_game);
        }
        if (key.ScanCode == RIGHT_ARROW_SCAN_CODE)
        {
          blockGameMovePieceRight(&block_game);
        }
        if (key.ScanCode == DOWN_ARROW_SCAN_CODE)
        {
          if(blockGameMovePieceDown(&block_game))
          {
            block_game.current_score++;
          }
        }
        if (key.UnicodeChar == L' ')
        {
          blockGameHardDrop(&block_game);
        }
      }
      if (key.UnicodeChar == L'q')
        {
          return EFI_SUCCESS;
        }
        if (key.UnicodeChar == L'r')
        {
          resetBlockGameGame(&block_game);
        }
    }

    block_game.level = block_game.current_score/8000;
    if (!block_game.is_game_finished)
    {
      int how_many_ticks_between_drop = (2 * desired_ticks_per_second);
      how_many_ticks_between_drop -= (2.0 * ((float)desired_ticks_per_second)/(20.0))*((float)block_game.level);
      if(how_many_ticks_between_drop < 1)
      {
        how_many_ticks_between_drop = 1;
      }
      if (tick_count % how_many_ticks_between_drop == 0)
      {
        blockGameMovePieceDown(&block_game);
      }
    }
    drawBlockGameGame(&block_game, &buf, 200, 100);
    drawBlockGameInfoTable(&block_game, &buf, 800, 100);
    bltGraphicBuffer(&block_game.graphics_data.help, &buf, 800, 400);
    uefi_call_wrapper(Gop->Blt, 10, Gop, buf.pixelArray, EfiBltBufferToVideo, 0, 0, 0, 0, buf.width, buf.height, NULL);
  }
  return EFI_SUCCESS;
}
