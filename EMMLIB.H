/*===========================================================================*/
/*                          EMM FUNCTION CLASSES                             */
/*------------------------------------+--------------------------------------*/
/*     PRESENCE, STATUS, & VERSION    |      MEMORY MOVEMENT & EXCHANGE      */
/*------------------------------------+--------------------------------------*/
/*   EMM_installed                    |   move_memory_region                 */
/*   get_EMM_status                   |   xchg_memory_region                 */
/*   get_EMM_version                  |                                      */
/*                                    |                                      */
/*------------------------------------+--------------------------------------*/
/*          MEMORY ALLOCATION         |           HANDLE MANAGEMENT          */
/*------------------------------------+--------------------------------------*/
/*   get_unalloc_page_count           |   get_handle_count                   */
/*   get_alloc_page_count             |   get_handle_pages                   */
/*   get_total_page_count             |   get_all_handles_pages              */
/*   get_unalloc_raw_page_count       |   get_handle_attrib                  */
/*   get_alloc_raw_page_count         |   set_handle_attrib                  */
/*   get_total_raw_page_count         |   get_attrib_capability              */
/*   alloc_pages                      |   get_handle_name                    */
/*   dealloc_pages                    |   set_handle_name                    */
/*   realloc_pages                    |   get_handle_dir                     */
/*   alloc_std_pages                  |   search_handle_name                 */
/*   alloc_raw_pages                  |   get_total_handles                  */
/*                                    |                                      */
/*------------------------------------+--------------------------------------*/
/*     MAPPABLE MEMORY REGION INFO    |         PROGRAM FLOW CONTROL         */
/*------------------------------------+--------------------------------------*/
/*   get_page_frame_seg               |   alter_map_jump                     */
/*   get_mappable_conv_regions        |   alter_map_call                     */
/*   get_mappable_exp_regions         |   get_alter_map_call_stack_size      */
/*   get_mappable_regions             |                                      */
/*   get_mappable_conv_region_count   |                                      */
/*   get_mappable_exp_region_count    |                                      */
/*   get_mappable_region_count        |                                      */
/*   get_page_frame_count             |                                      */
/*                                    |                                      */
/*------------------------------------+--------------------------------------*/
/*           MEMORY MAPPING           |                OS ONLY               */
/*------------------------------------+--------------------------------------*/
/*   map_unmap_page                   |   enable_OS_fcns                     */
/*   map_unmap_pages                  |   disable_OS_fcns                    */
/*                                    |   return_OS_access_key               */
/*------------------------------------+   get_hw_info                        */
/*       MEMORY MAPPING CONTEXT       |   get_alt_reg_set                    */
/*------------------------------------+   set_alt_reg_set                    */
/*   save_context                     |   get_alt_context_size               */
/*   restore_context                  |   alloc_alt_reg_set                  */
/*   get_context                      |   dealloc_alt_reg_set                */
/*   set_context                      |   alloc_DMA_reg_set                  */
/*   get_set_context                  |   enable_DMA_reg_set                 */
/*   get_context_size                 |   disable_DMA_reg_set                */
/*   get_partial_context              |   dealloc_DMA_reg_set                */
/*   get_partial_context_size         |   prep_EMM_warmboot                  */
/*   set_partial_context              |                                      */
/*                                    |                                      */
/*====================================+======================================*/
  
/*===========================================================================*/
/*                           EMM MANIFEST CONSTANTS                          */
/*===========================================================================*/
#define MAX_HANDLES             255
#define MAX_HANDLE_NAME_LEN     8
#define VOLATILE                0
#define NONVOLATILE             1
#define MAX_MAPPABLE_REGIONS    64
#define PHYS_PAGE_MODE          0
#define SEG_MODE                1
#define MAX_CONTEXT_SIZE        255
#define CONV_MEM                0
#define EXP_MEM                 1
#define UNMAP                   0xFFFF
  
/*===========================================================================*/
/*                                 EMM MACRO                                 */
/*===========================================================================*/
    /*-----------------------------------------------------------------------*/
    /*   This macro converts an unsigned int containing a segment address,   */
    /*   into a far pointer to a void.  It is useful in converting the       */
    /*   segment values, returned by EMM function calls, into far pointers   */
    /*   which C programs can use.  For example:                             */
    /*                                                                       */
    /*   unsigned int status,                                                */
    /*                  page_frame_seg;                                      */
    /*   char far *page_frame_ptr;                                           */
    /*                                                                       */
    /*   status = get_page_frame_seg (&page_frame_seg);                      */
    /*   page_frame_ptr = FP(page_frame_seg);                                */
    /*   *page_frame_ptr = 0;                                                */
    /*-----------------------------------------------------------------------*/
    #define FP(SEG) ((void far *)((unsigned long)SEG << 16))
  
/*===========================================================================*/
/*                           EMM STRUCTURE TYPEDEFS                          */
/*===========================================================================*/
    /*-----------------------------------------------------------------------*/
    /*               Structure:   HANDLES_PAGES_STRUCT                       */
    /*                                                                       */
    /*   Required by Functions:   get_all_handles_pages                      */
    /*-----------------------------------------------------------------------*/
    #pragma pack(1)
typedef struct
{
    unsigned int            handle;
    unsigned int            pages_allocated;
} HANDLES_PAGES_STRUCT;
    #pragma pack()
  
    /*-----------------------------------------------------------------------*/
    /*               Structure:   HANDLE_NAME_STRUCT                         */
    /*                                                                       */
    /*   Required by Functions:   get_handle_name                            */
    /*                            set_handle_name                            */
    /*                            search_handle_name                         */
    /*-----------------------------------------------------------------------*/
    #pragma pack(1)
typedef struct
{
    unsigned char           name [MAX_HANDLE_NAME_LEN];
} HANDLE_NAME_STRUCT;
    #pragma pack()
  
    /*-----------------------------------------------------------------------*/
    /*               Structure:   HANDLE_DIR_STRUCT                          */
    /*                                                                       */
    /*   Required by Functions:   get_handle_dir                             */
    /*-----------------------------------------------------------------------*/
    #pragma pack(1)
typedef struct
{
    unsigned int            handle;
    unsigned char           name [MAX_HANDLE_NAME_LEN];
} HANDLE_DIR_STRUCT;
    #pragma pack()
  
    /*-----------------------------------------------------------------------*/
    /*               Structure:   CONTEXT_STRUCT                             */
    /*                                                                       */
    /*   Required by Functions:   get_context                                */
    /*                            set_context                                */
    /*                            get_set_context                            */
    /*                            get_partial_context                        */
    /*                            set_partial_context                        */
    /*                            set_alt_reg_set                            */
    /*-----------------------------------------------------------------------*/
    #pragma pack(1)
typedef struct
{
    unsigned char           reserved [MAX_CONTEXT_SIZE];
} CONTEXT_STRUCT;
    #pragma pack()
  
    /*-----------------------------------------------------------------------*/
    /*               Structure:   PARTIAL_CONTEXT_LIST_STRUCT                */
    /*                                                                       */
    /*   Required by Functions:   get_partial_context                        */
    /*-----------------------------------------------------------------------*/
    #pragma pack(1)
typedef struct
{
    unsigned int            mappable_region_count;
    unsigned int            mappable_region_seg [MAX_MAPPABLE_REGIONS];
} PARTIAL_CONTEXT_LIST_STRUCT;
    #pragma pack()
  
    /*-----------------------------------------------------------------------*/
    /*               Structure:   MAP_STRUCT                                 */
    /*                                                                       */
    /*   Required by Functions:   map_unmap_pages                            */
    /*                            alter_map_jump                             */
    /*                            alter_map_call                             */
    /*-----------------------------------------------------------------------*/
    #pragma pack(1)
typedef struct
{
    unsigned int            log_page;
    unsigned int            phys_page_or_seg;
} MAP_STRUCT;
    #pragma pack()
  
    /*-----------------------------------------------------------------------*/
    /*               Structure:   MAP_JUMP_STRUCT                            */
    /*                                                                       */
    /*   Required by Functions:   alter_map_jump                             */
    /*-----------------------------------------------------------------------*/
    #pragma pack(1)
typedef struct
{
    void                    (far *target_function) ();
    unsigned char           map_struct_count;
    MAP_STRUCT far *        ptr_map_struct;
} MAP_JUMP_STRUCT;
    #pragma pack()
  
    /*-----------------------------------------------------------------------*/
    /*               Structure:   MAP_CALL_STRUCT                            */
    /*                                                                       */
    /*   Required by Functions:   alter_map_call                             */
    /*-----------------------------------------------------------------------*/
    #pragma pack(1)
typedef struct
{
    void                    (far *target_function) ();
    unsigned char           init_map_struct_count;
    MAP_STRUCT far *        ptr_init_map_struct;
    unsigned char           final_map_struct_count;
    MAP_STRUCT far *        ptr_final_map_struct;
    unsigned char           reserved[8];
} MAP_CALL_STRUCT;
    #pragma pack()
  
    /*-----------------------------------------------------------------------*/
    /*               Structure:   MOVE_XCHG_STRUCT                           */
    /*                                                                       */
    /*   Required by Functions:   move_memory_region                         */
    /*                            xchg_memory_region                         */
    /*-----------------------------------------------------------------------*/
    #pragma pack(1)
typedef struct
{
    unsigned long           region_size;
    unsigned char           source_mem_type;
    unsigned int            source_handle;
    unsigned int            source_init_offset;
    unsigned int            source_init_log_page_or_seg;
    unsigned char           dest_mem_type;
    unsigned int            dest_handle;
    unsigned int            dest_init_offset;
    unsigned int            dest_init_log_page_or_seg;
} MOVE_XCHG_STRUCT;
    #pragma pack()
  
    /*-----------------------------------------------------------------------*/
    /*               Structure:   MAPPABLE_REGION_STRUCT                     */
    /*                                                                       */
    /*   Required by Functions:   get_mappable_conv_regions                  */
    /*                            get_mappable_exp_regions                   */
    /*                            get_mappable_regions                       */
    /*-----------------------------------------------------------------------*/
    #pragma pack(1)
typedef struct
{
    unsigned int            mappable_region_seg;
    unsigned int            phys_page;
} MAPPABLE_REGION_STRUCT;
    #pragma pack()
  
    /*-----------------------------------------------------------------------*/
    /*               Structure:   HW_INFO_STRUCT                             */
    /*                                                                       */
    /*   Required by Functions:   get_hw_info                                */
    /*-----------------------------------------------------------------------*/
    #pragma pack(1)
typedef struct
{
    unsigned int            raw_page_size_paragraphs;
    unsigned int            alt_reg_set_count;
    unsigned int            context_size;
    unsigned int            DMA_reg_set_count;
    unsigned int            DMA_channel_operation;
} HW_INFO_STRUCT;
    #pragma pack()
  
/*===========================================================================*/
/*                          EMM FUNCTION PROTOTYPES                          */
/*===========================================================================*/
    /*-----------------------------------------------------------------------*/
    /*   unsigned int status;                                                */
    /*                                                                       */
    /*   status = EMM_installed ();                                          */
    /*-----------------------------------------------------------------------*/
unsigned int EMM_installed  (void);
  
    /*-----------------------------------------------------------------------*/
    /*   unsigned int status;                                                */
    /*                                                                       */
    /*   status = get_EMM_status ();                                         */
    /*-----------------------------------------------------------------------*/
unsigned int get_EMM_status (void);
  
    /*-----------------------------------------------------------------------*/
    /*   unsigned int status,                                                */
    /*                page_frame_seg;                                        */
    /*                                                                       */
    /*   status = get_page_frame_seg (&page_frame_seg);                      */
    /*-----------------------------------------------------------------------*/
unsigned int get_page_frame_seg (unsigned int far *);
  
    /*-----------------------------------------------------------------------*/
    /*   unsigned int status,                                                */
    /*                unalloc_page_count;                                    */
    /*                                                                       */
    /*   status = get_unalloc_page_count (&unalloc_page_count);              */
    /*-----------------------------------------------------------------------*/
unsigned int get_unalloc_page_count (unsigned int far *);
  
    /*-----------------------------------------------------------------------*/
    /*   unsigned int status,                                                */
    /*                alloc_page_count;                                      */
    /*                                                                       */
    /*   status = get_alloc_page_count (&alloc_page_count);                  */
    /*-----------------------------------------------------------------------*/
unsigned int get_alloc_page_count   (unsigned int far *);
  
    /*-----------------------------------------------------------------------*/
    /*   unsigned int status,                                                */
    /*                total_page_count;                                      */
    /*                                                                       */
    /*   status = get_total_page_count (&total_page_count);                  */
    /*-----------------------------------------------------------------------*/
unsigned int get_total_page_count   (unsigned int far *);
  
    /*-----------------------------------------------------------------------*/
    /*   unsigned int status,                                                */
    /*                pages,                                                 */
    /*                handle;                                                */
    /*                                                                       */
    /*   status = alloc_pages (pages,                                        */
    /*                         &handle);                                     */
    /*-----------------------------------------------------------------------*/
unsigned int alloc_pages    (unsigned int,
unsigned int far *);
  
    /*-----------------------------------------------------------------------*/
    /*   unsigned int status,                                                */
    /*                phys_page,                                             */
    /*                log_page,                                              */
    /*                handle;                                                */
    /*                                                                       */
    /*   status = map_unmap_page (phys_page,                                 */
    /*                            log_page,                                  */
    /*                            handle);                                   */
    /*-----------------------------------------------------------------------*/
unsigned int map_unmap_page (unsigned int,
unsigned int,
unsigned int);
  
    /*-----------------------------------------------------------------------*/
    /*   unsigned int status,                                                */
    /*                handle;                                                */
    /*                                                                       */
    /*   status = dealloc_pages (handle);                                    */
    /*-----------------------------------------------------------------------*/
unsigned int dealloc_pages  (unsigned int);
  
    /*-----------------------------------------------------------------------*/
    /*   unsigned int status,                                                */
    /*                version;                                               */
    /*                                                                       */
    /*   status = get_EMM_version (&version);                                */
    /*-----------------------------------------------------------------------*/
unsigned int get_EMM_version    (unsigned int far *);
  
    /*-----------------------------------------------------------------------*/
    /*   unsigned int status,                                                */
    /*                handle;                                                */
    /*                                                                       */
    /*   status = save_context (handle);                                     */
    /*-----------------------------------------------------------------------*/
unsigned int save_context (unsigned int);
  
    /*-----------------------------------------------------------------------*/
    /*   unsigned int status,                                                */
    /*                handle;                                                */
    /*                                                                       */
    /*   status = restore_context (handle);                                  */
    /*-----------------------------------------------------------------------*/
unsigned int restore_context    (unsigned int);
  
    /*-----------------------------------------------------------------------*/
    /*   unsigned int status,                                                */
    /*                handle_count;                                          */
    /*                                                                       */
    /*   status = get_handle_count (&handle_count);                          */
    /*-----------------------------------------------------------------------*/
unsigned int get_handle_count   (unsigned int far *);
  
    /*-----------------------------------------------------------------------*/
    /*   unsigned int status,                                                */
    /*                pages_alloc_to_handle,                                 */
    /*                handle;                                                */
    /*                                                                       */
    /*   status = get_handle_pages (&pages_alloc_to_handle,                  */
    /*                              handle);                                 */
    /*-----------------------------------------------------------------------*/
unsigned int get_handle_pages   (unsigned int far *,
unsigned int);
  
    /*-----------------------------------------------------------------------*/
    /*   unsigned             int status,                                    */
    /*                        hp_count;                                      */
    /*   HANDLES_PAGES_STRUCT hp [MAX_HANDLES];                              */
    /*                                                                       */
    /*   status = get_all_handles_pages (&hp_count,                          */
    /*                                   hp);                                */
    /*-----------------------------------------------------------------------*/
unsigned int get_all_handles_pages  (unsigned int far *,
HANDLES_PAGES_STRUCT far *);
  
    /*-----------------------------------------------------------------------*/
    /*   unsigned int   status;                                              */
    /*   CONTEXT_STRUCT dest_context;                                        */
    /*                                                                       */
    /*   status = get_context (&dest_context);                               */
    /*-----------------------------------------------------------------------*/
unsigned int get_context    (CONTEXT_STRUCT far *);
  
    /*-----------------------------------------------------------------------*/
    /*   unsigned int   status;                                              */
    /*   CONTEXT_STRUCT source_context;                                      */
    /*                                                                       */
    /*   status = set_context (&source_context);                             */
    /*-----------------------------------------------------------------------*/
unsigned int set_context    (CONTEXT_STRUCT far *);
  
    /*-----------------------------------------------------------------------*/
    /*   unsigned int   status;                                              */
    /*   CONTEXT_STRUCT dest_context;                                        */
    /*   CONTEXT_STRUCT source_context;                                      */
    /*                                                                       */
    /*   status = get_set_context (&dest_context,                            */
    /*                             &source_context);                         */
    /*-----------------------------------------------------------------------*/
unsigned int get_set_context    (CONTEXT_STRUCT far *,
CONTEXT_STRUCT far *);
  
    /*-----------------------------------------------------------------------*/
    /*   unsigned int status,                                                */
    /*                context_size;                                          */
    /*                                                                       */
    /*   status = get_context_size (&context_size);                          */
    /*-----------------------------------------------------------------------*/
unsigned int get_context_size   (unsigned int far *);
  
    /*-----------------------------------------------------------------------*/
    /*   unsigned int                status;                                 */
    /*   PARTIAL_CONTEXT_LIST_STRUCT pcl;                                    */
    /*   CONTEXT_STRUCT              dest_context;                           */
    /*                                                                       */
    /*   pcl.mappable_region_count = 2;                                      */
    /*   pcl.mappable_region_seg   = 0xC000;                                 */
    /*   pcl.mappable_region_seg   = 0xC400;                                 */
    /*   status = get_partial_context (&pcl,                                 */
    /*                                 &dest_context);                       */
    /*-----------------------------------------------------------------------*/
unsigned int get_partial_context    (PARTIAL_CONTEXT_LIST_STRUCT far *,
CONTEXT_STRUCT far *);
  
    /*-----------------------------------------------------------------------*/
    /*   unsigned int   status;                                              */
    /*   CONTEXT_STRUCT source_context;                                      */
    /*                                                                       */
    /*   status = set_partial_context (&source_context);                     */
    /*-----------------------------------------------------------------------*/
unsigned int set_partial_context    (CONTEXT_STRUCT far *);
  
    /*-----------------------------------------------------------------------*/
    /*   unsigned int                status,                                 */
    /*                               partial_context_size;                   */
    /*   PARTIAL_CONTEXT_LIST_STRUCT pcl;                                    */
    /*                                                                       */
    /*   pcl.mappable_region_count = 2;                                      */
    /*   status = get_partial_context_size (pcl.mappable_region_count,       */
    /*                                      &partial_context_size);          */
    /*-----------------------------------------------------------------------*/
unsigned int get_partial_context_size   (unsigned int,
unsigned int far *);
  
    /*-----------------------------------------------------------------------*/
    /*   unsigned int status,                                                */
    /*                mode,                                                  */
    /*                mu_count,                                              */
    /*                handle;                                                */
    /*   MAP_STRUCT   mu [MAX_MAPPABLE_REGIONS];                             */
    /*                                                                       */
    /*   mode                   = SEG_MODE;                                  */
    /*   mu_count               = 2;                                         */
    /*   mu[0].log_page         = 0;                                         */
    /*   mu[0].phys_page_or_seg = 0xC000;                                    */
    /*   mu[1].log_page         = 1;                                         */
    /*   mu[1].phys_page_or_seg = 0xC400;                                    */
    /*   status = map_unmap_pages (mode,                                     */
    /*                             mu_count,                                 */
    /*                             mu,                                       */
    /*                             handle);                                  */
    /*-----------------------------------------------------------------------*/
unsigned int map_unmap_pages    (unsigned int,
unsigned int,
MAP_STRUCT far *,
unsigned int);
  
    /*-----------------------------------------------------------------------*/
    /*   unsigned int status,                                                */
    /*                pages,                                                 */
    /*                handle;                                                */
    /*                                                                       */
    /*   status = realloc_pages (&pages,                                     */
    /*                           handle);                                    */
    /*-----------------------------------------------------------------------*/
unsigned int realloc_pages  (unsigned int far *,
unsigned int);
  
    /*-----------------------------------------------------------------------*/
    /*   unsigned int status,                                                */
    /*                attrib,                                                */
    /*                handle;                                                */
    /*                                                                       */
    /*   status = get_handle_attrib (&attrib,                                */
    /*                               handle);                                */
    /*-----------------------------------------------------------------------*/
unsigned int get_handle_attrib  (unsigned int far *,
unsigned int);
  
    /*-----------------------------------------------------------------------*/
    /*   unsigned int status,                                                */
    /*                attrib,                                                */
    /*                handle;                                                */
    /*                                                                       */
    /*   attrib = VOLATILE;                                                  */
    /*   status = set_handle_attrib (attrib,                                 */
    /*                               handle);                                */
    /*-----------------------------------------------------------------------*/
unsigned int set_handle_attrib  (unsigned int,
unsigned int);
  
    /*-----------------------------------------------------------------------*/
    /*   unsigned int status,                                                */
    /*                attrib_capability;                                     */
    /*                                                                       */
    /*   status = get_attrib_capability (&attrib_capability);                */
    /*-----------------------------------------------------------------------*/
unsigned int get_attrib_capability  (unsigned int far *);
  
    /*-----------------------------------------------------------------------*/
    /*   unsigned int       status,                                          */
    /*                      handle;                                          */
    /*   HANDLE_NAME_STRUCT hn;                                              */
    /*                                                                       */
    /*   status = get_handle_name (&hn,                                      */
    /*                             handle);                                  */
    /*-----------------------------------------------------------------------*/
unsigned int get_handle_name    (HANDLE_NAME_STRUCT far *,
unsigned int);
  
    /*-----------------------------------------------------------------------*/
    /*   unsigned int       status,                                          */
    /*                      handle;                                          */
    /*   HANDLE_NAME_STRUCT hn;                                              */
    /*                                                                       */
    /*   status = set_handle_name (&hn,                                      */
    /*                             handle);                                  */
    /*-----------------------------------------------------------------------*/
unsigned int set_handle_name    (HANDLE_NAME_STRUCT far *,
unsigned int);
  
    /*-----------------------------------------------------------------------*/
    /*   unsigned int      status,                                           */
    /*                     hd_count;                                         */
    /*   HANDLE_DIR_STRUCT hd [MAX_HANDLES];                                 */
    /*                                                                       */
    /*   status = get_handle_dir (&hd_count,                                 */
    /*                            hd);                                       */
    /*-----------------------------------------------------------------------*/
unsigned int get_handle_dir (unsigned int far *,
HANDLE_DIR_STRUCT far *);
  
    /*-----------------------------------------------------------------------*/
    /*   unsigned int       status,                                          */
    /*                      handle;                                          */
    /*   HANDLE_NAME_STRUCT hn;                                              */
    /*                                                                       */
    /*   status = search_handle_name (&hn,                                   */
    /*                                &handle);                              */
    /*-----------------------------------------------------------------------*/
unsigned int search_handle_name (HANDLE_NAME_STRUCT far *,
unsigned int far *);
  
    /*-----------------------------------------------------------------------*/
    /*   unsigned int status,                                                */
    /*                max_handle_count;                                      */
    /*                                                                       */
    /*   status = get_total_handles (&max_handle_count);                     */
    /*-----------------------------------------------------------------------*/
unsigned int get_total_handles  (unsigned int far *);
  
    /*-----------------------------------------------------------------------*/
    /*   unsigned int    status,                                             */
    /*                   mode,                                               */
    /*                   handle;                                             */
    /*   MAP_STRUCT      m [MAX_MAPPABLE_REGIONS];                           */
    /*   MAP_JUMP_STRUCT mj;                                                 */
    /*                                                                       */
    /*   m[0].log_page         = 0;                                          */
    /*   m[0].phys_page_or_seg = 0xC000;                                     */
    /*   m[1].log_page         = 1;                                          */
    /*   m[1].phys_page_or_seg = 0xC400;                                     */
    /*   m[2].log_page         = 2;                                          */
    /*   m[2].phys_page_or_seg = 0xC800;                                     */
    /*   m[3].log_page         = 3;                                          */
    /*   m[3].phys_page_or_seg = 0xCC00;                                     */
    /*                                                                       */
    /*   mode                = SEG_MODE;                                     */
    /*   mj.target_function  = 0xC0000000;                                   */
    /*   mj.map_struct_count = 4;                                            */
    /*   mj.ptr_map_struct   = m;                                            */
    /*                                                                       */
    /*   status = alter_map_jump (mode,                                      */
    /*                            &mj,                                       */
    /*                            handle);                                   */
    /*-----------------------------------------------------------------------*/
unsigned int alter_map_jump (unsigned int,
MAP_JUMP_STRUCT far *,
unsigned int);
  
    /*-----------------------------------------------------------------------*/
    /*   unsigned int    status,                                             */
    /*                   mode,                                               */
    /*                   handle;                                             */
    /*   MAP_STRUCT      init_map  [MAX_MAPPABLE_REGIONS];                   */
    /*   MAP_STRUCT      final_map [MAX_MAPPABLE_REGIONS];                   */
    /*   MAP_CALL_STRUCT mc;                                                 */
    /*                                                                       */
    /*   imit_map[0].log_page          = 0;                                  */
    /*   init_map[0].phys_page_or_seg  = 0xC000;                             */
    /*                                                                       */
    /*   final_map[0].log_page         = 1;                                  */
    /*   final_map[0].phys_page_or_seg = 0xC000;                             */
    /*   final_map[1].log_page         = 2;                                  */
    /*   final_map[1].phys_page_or_seg = 0xC400;                             */
    /*                                                                       */
    /*   mode = SEG_MODE;                                                    */
    /*   mc.target_function        = 0xC0000000;                             */
    /*   mc.init_map_struct_count  = 1;                                      */
    /*   mc.ptr_init_map_struct    = init_map;                               */
    /*   mc.final_map_struct_count = 2;                                      */
    /*   mc.ptr_final_map_struct   = final_map;                              */
    /*                                                                       */
    /*   status = alter_map_call (mode,                                      */
    /*                            &mc,                                       */
    /*                            handle);                                   */
    /*-----------------------------------------------------------------------*/
unsigned int alter_map_call (unsigned int,
MAP_CALL_STRUCT far *,
unsigned int);
  
    /*-----------------------------------------------------------------------*/
    /*   unsigned int status,                                                */
    /*                call_stack_space_size;                                 */
    /*                                                                       */
    /*   status = get_alter_map_call_stack_size (&call_stack_space_size);    */
    /*-----------------------------------------------------------------------*/
unsigned int get_alter_map_call_stack_size  (unsigned int far *);
  
    /*-----------------------------------------------------------------------*/
    /*   unsigned int     status;                                            */
    /*   MOVE_XCHG_STRUCT ms;                                                */
    /*                                                                       */
    /*   ms.region_size                 = 640 * 1024;                        */
    /*   ms.source_mem_type             = CONV_MEM;                          */
    /*   ms.source_init_log_page_or_seg = 0x0000;                            */
    /*   ms.source_init_offset          = 0x0000;                            */
    /*   ms.dest_mem_type               = EXP_MEM;                           */
    /*   ms.dest_handle                 = 1;                                 */
    /*   ms.dest_init_log_page_or_seg   = 0;                                 */
    /*   ms.dest_init_offset            = 0x0000;                            */
    /*                                                                       */
    /*   status = move_memory_region (&ms);                                  */
    /*-----------------------------------------------------------------------*/
unsigned int move_memory_region         (MOVE_XCHG_STRUCT far *);
  
    /*-----------------------------------------------------------------------*/
    /*   unsigned int     status;                                            */
    /*   MOVE_XCHG_STRUCT xs;                                                */
    /*                                                                       */
    /*   xs.region_size                 = 640 * 1024;                        */
    /*   xs.source_mem_type             = EXP_MEM;                           */
    /*   xs.source_handle               = 1;                                 */
    /*   xs.source_init_log_page_or_seg = 0;                                 */
    /*   xs.source_init_offset          = 0x0000;                            */
    /*   xs.dest_mem_type               = CONV_MEM;                          */
    /*   xs.dest_init_log_page_or_seg   = 0x0000;                            */
    /*   xs.dest_init_offset            = 0x0000;                            */
    /*                                                                       */
    /*   status = xchg_memory_region (&xs);                                  */
    /*-----------------------------------------------------------------------*/
unsigned int xchg_memory_region         (MOVE_XCHG_STRUCT far *);
  
    /*-----------------------------------------------------------------------*/
    /*   unsigned int           status,                                      */
    /*                          mcr_count;                                   */
    /*   MAPPABLE_REGION_STRUCT mcr [MAX_MAPPABLE_REGIONS];                  */
    /*                                                                       */
    /*   status = get_mappable_conv_regions (&mcr_count,                     */
    /*                                       mcr);                           */
    /*-----------------------------------------------------------------------*/
unsigned int get_mappable_conv_regions  (unsigned int far *,
MAPPABLE_REGION_STRUCT far *);
  
    /*-----------------------------------------------------------------------*/
    /*   unsigned int           status,                                      */
    /*                          mer_count;                                   */
    /*   MAPPABLE_REGION_STRUCT mer [MAX_MAPPABLE_REGIONS];                  */
    /*                                                                       */
    /*   status = get_mappable_exp_regions (&mer_count,                      */
    /*                                      mer);                            */
    /*-----------------------------------------------------------------------*/
unsigned int get_mappable_exp_regions       (unsigned int far *,
MAPPABLE_REGION_STRUCT far *);
  
    /*-----------------------------------------------------------------------*/
    /*   unsigned int           status,                                      */
    /*                          mr_count;                                    */
    /*   MAPPABLE_REGION_STRUCT mr [MAX_MAPPABLE_REGIONS];                   */
    /*                                                                       */
    /*   status = get_mappable_regions (&mr_count,                           */
    /*                                  mr);                                 */
    /*-----------------------------------------------------------------------*/
unsigned int get_mappable_regions           (unsigned int far *,
MAPPABLE_REGION_STRUCT far *);
  
    /*-----------------------------------------------------------------------*/
    /*   unsigned int status,                                                */
    /*                mcr_count;                                             */
    /*                                                                       */
    /*   status = get_mappable_conv_region_count (&mcr_count);               */
    /*-----------------------------------------------------------------------*/
unsigned int get_mappable_conv_region_count (unsigned int far *);
  
    /*-----------------------------------------------------------------------*/
    /*   unsigned int status,                                                */
    /*                mer_count;                                             */
    /*                                                                       */
    /*   status = get_mappable_exp_region_count (&mer_count);                */
    /*-----------------------------------------------------------------------*/
unsigned int get_mappable_exp_region_count  (unsigned int far *);
  
    /*-----------------------------------------------------------------------*/
    /*   unsigned int status,                                                */
    /*                mr_count;                                              */
    /*                                                                       */
    /*   status = get_mappable_region_count (&mr_count);                     */
    /*-----------------------------------------------------------------------*/
unsigned int get_mappable_region_count  (unsigned int far *);
  
    /*-----------------------------------------------------------------------*/
    /*   unsigned int status,                                                */
    /*                pf_count;                                              */
    /*                                                                       */
    /*   status = get_page_frame_count (&pf_count);                          */
    /*-----------------------------------------------------------------------*/
unsigned int get_page_frame_count   (unsigned int far *);
  
    /*-----------------------------------------------------------------------*/
    /*   unsigned int   status                                               */
    /*   HW_INFO_STRUCT hw_info;                                             */
    /*                                                                       */
    /*   status = get_hw_info (&hw_info);                                    */
    /*-----------------------------------------------------------------------*/
unsigned int get_hw_info                    (HW_INFO_STRUCT far *);
  
    /*-----------------------------------------------------------------------*/
    /*   unsigned int status,                                                */
    /*                unalloc_raw_pages;                                     */
    /*                                                                       */
    /*   status = get_unalloc_raw_page_count (&unalloc_raw_pages);           */
    /*-----------------------------------------------------------------------*/
unsigned int get_unalloc_raw_page_count (unsigned int far *);
  
    /*-----------------------------------------------------------------------*/
    /*   unsigned int status,                                                */
    /*                alloc_raw_pages;                                       */
    /*                                                                       */
    /*   status = get_alloc_raw_page_count (&alloc_raw_pages);               */
    /*-----------------------------------------------------------------------*/
unsigned int get_alloc_raw_page_count       (unsigned int far *);
  
    /*-----------------------------------------------------------------------*/
    /*   unsigned int status,                                                */
    /*                total_raw_pages;                                       */
    /*                                                                       */
    /*   status = get_total_raw_page_count (&total_raw_pages);               */
    /*-----------------------------------------------------------------------*/
unsigned int get_total_raw_page_count       (unsigned int far *);
  
    /*-----------------------------------------------------------------------*/
    /*   unsigned int status,                                                */
    /*                std_page_count,                                        */
    /*                handle;                                                */
    /*                                                                       */
    /*   status = alloc_std_pages (std_page_count,                           */
    /*                             &handle);                                 */
    /*-----------------------------------------------------------------------*/
unsigned int alloc_std_pages                (unsigned int,
unsigned int far *);
  
    /*-----------------------------------------------------------------------*/
    /*   unsigned int status,                                                */
    /*                raw_page_count,                                        */
    /*                handle;                                                */
    /*                                                                       */
    /*   status = alloc_raw_pages (raw_page_count,                           */
    /*                             &handle);                                 */
    /*-----------------------------------------------------------------------*/
unsigned int alloc_raw_pages                (unsigned int,
unsigned int far *);
  
    /*-----------------------------------------------------------------------*/
    /*   unsigned int       status,                                          */
    /*                      alt_reg_set;                                     */
    /*   CONTEXT_STRUCT far *far_ptr_alt_context;                            */
    /*                                                                       */
    /*   status = get_alt_reg_set (&alt_reg_set,                             */
    /*                             &far_ptr_alt_context);                    */
    /*-----------------------------------------------------------------------*/
unsigned int get_alt_reg_set        (unsigned int far *,
CONTEXT_STRUCT far * far *);
  
    /*-----------------------------------------------------------------------*/
    /*   unsigned int          status,                                       */
    /*                         alt_reg_set;                                  */
    /*   static CONTEXT_STRUCT alt_context;                                  */
    /*                                                                       */
    /*   status = set_alt_reg_set (alt_reg_set,                              */
    /*                             &alt_context);                            */
    /*-----------------------------------------------------------------------*/
unsigned int set_alt_reg_set        (unsigned int,
CONTEXT_STRUCT far *);
  
    /*-----------------------------------------------------------------------*/
    /*   unsigned int status,                                                */
    /*                context_size;                                          */
    /*                                                                       */
    /*   status = get_alt_context_size (&context_size);                      */
    /*-----------------------------------------------------------------------*/
unsigned int get_alt_context_size   (unsigned int far *);
  
    /*-----------------------------------------------------------------------*/
    /*   unsigned int status,                                                */
    /*                alt_reg_set;                                           */
    /*                                                                       */
    /*   status = alloc_alt_reg_set (&alt_reg_set);                          */
    /*-----------------------------------------------------------------------*/
unsigned int alloc_alt_reg_set  (unsigned int far *);
  
    /*-----------------------------------------------------------------------*/
    /*   unsigned int status,                                                */
    /*                alt_reg_set;                                           */
    /*                                                                       */
    /*   status = dealloc_alt_reg_set (alt_reg_set);                         */
    /*-----------------------------------------------------------------------*/
unsigned int dealloc_alt_reg_set    (unsigned int);
  
    /*-----------------------------------------------------------------------*/
    /*   unsigned int status,                                                */
    /*                DMA_reg_set;                                           */
    /*                                                                       */
    /*   status = alloc_DMA_reg_set (&DMA_reg_set);                          */
    /*-----------------------------------------------------------------------*/
unsigned int alloc_DMA_reg_set  (unsigned int far *);
  
    /*-----------------------------------------------------------------------*/
    /*   unsigned int status,                                                */
    /*                DMA_reg_set,                                           */
    /*                DMA_channel;                                           */
    /*                                                                       */
    /*   status = enable_DMA_reg_set (DMA_reg_set,                           */
    /*                                DMA_channel);                          */
    /*-----------------------------------------------------------------------*/
unsigned int enable_DMA_reg_set (unsigned int,
unsigned int);
  
    /*-----------------------------------------------------------------------*/
    /*   unsigned int status,                                                */
    /*                DMA_reg_set;                                           */
    /*                                                                       */
    /*   status = disable_DMA_reg_set (DMA_reg_set);                         */
    /*-----------------------------------------------------------------------*/
unsigned int disable_DMA_reg_set    (unsigned int);
  
    /*-----------------------------------------------------------------------*/
    /*   unsigned int status,                                                */
    /*                DMA_reg_set;                                           */
    /*                                                                       */
    /*   status = dealloc_DMA_reg_set (DMA_reg_set);                         */
    /*-----------------------------------------------------------------------*/
unsigned int dealloc_DMA_reg_set    (unsigned int);
  
    /*-----------------------------------------------------------------------*/
    /*   unsigned int status;                                                */
    /*                                                                       */
    /*   status = prep_EMM_warmboot();                                       */
    /*-----------------------------------------------------------------------*/
unsigned int prep_EMM_warmboot      (void);
  
    /*-----------------------------------------------------------------------*/
    /*   unsigned int  status;                                               */
    /*   unsigned long access_key;                                           */
    /*                                                                       */
    /*   status = enable_OS_fcns (&access_key);                              */
    /*-----------------------------------------------------------------------*/
unsigned int enable_OS_fcns         (unsigned long far *);
  
    /*-----------------------------------------------------------------------*/
    /*   unsigned int  status                                                */
    /*   unsigned long access_key;                                           */
    /*                                                                       */
    /*   status = disable_OS_fcns (&access_key);                             */
    /*-----------------------------------------------------------------------*/
unsigned int disable_OS_fcns            (unsigned long far *);
  
    /*-----------------------------------------------------------------------*/
    /*   unsigned int  status                                                */
    /*   unsigned long access_key;                                           */
    /*                                                                       */
    /*   status = return_OS_access_key (access_key);                         */
    /*-----------------------------------------------------------------------*/
unsigned int return_OS_access_key       (unsigned long far *);
  
