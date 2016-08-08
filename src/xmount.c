/*******************************************************************************
* xmount Copyright (c) 2008-2016 by Gillen Daniel <gillen.dan@pinguin.lu>      *
*                                                                              *
* xmount is a small tool to "fuse mount" various harddisk image formats as dd, *
* vdi, vhd or vmdk files and enable virtual write access to them.              *
*                                                                              *
* This program is free software: you can redistribute it and/or modify it      *
* under the terms of the GNU General Public License as published by the Free   *
* Software Foundation, either version 3 of the License, or (at your option)    *
* any later version.                                                           *
*                                                                              *
* This program is distributed in the hope that it will be useful, but WITHOUT  *
* ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or        *
* FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for     *
* more details.                                                                *
*                                                                              *
* You should have received a copy of the GNU General Public License along with *
* this program. If not, see <http://www.gnu.org/licenses/>.                    *
*******************************************************************************/

#include <config.h>

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <inttypes.h> // For PRI*
#include <errno.h>
#include <dlfcn.h> // For dlopen, dlclose, dlsym
#include <dirent.h> // For opendir, readdir, closedir
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/stat.h> // For fstat
#include <sys/types.h>
#ifdef HAVE_LINUX_FS_H
  #include <linux/fs.h> // For SEEK_* ??
#endif
#if !defined(__APPLE__) && defined(HAVE_GRP_H) && defined(HAVE_PWD_H)
  #include <grp.h> // For getgrnam, struct group
  #include <pwd.h> // For getpwuid, struct passwd
#endif
#include <pthread.h>
#include <time.h> // For time

#include "xmount.h"
#include "xmount_fuse.h"
#include "md5.h"
#include "macros.h"
#include "../libxmount/libxmount.h"

#define XMOUNT_COPYRIGHT_NOTICE \
  "xmount v%s Copyright (c) 2008-2016 by Gillen Daniel <gillen.dan@pinguin.lu>"

#define LOG_WARNING(...) {            \
  LIBXMOUNT_LOG_WARNING(__VA_ARGS__); \
}
#define LOG_ERROR(...) {            \
  LIBXMOUNT_LOG_ERROR(__VA_ARGS__); \
}
#define LOG_DEBUG(...) {                              \
  LIBXMOUNT_LOG_DEBUG(glob_xmount.debug,__VA_ARGS__); \
}

#define HASH_AMOUNT (1024*1024)*10 // Amount of data used to construct a
                                   // "unique" hash for every input image
                                   // (10MByte)

/*******************************************************************************
 * Forward declarations
 ******************************************************************************/
/*
 * Misc
 */
static void InitResources();
static void FreeResources();
static void PrintUsage(char*);
static void CheckFuseSettings();
static int ParseCmdLine(const int, char**);
static int ExtractOutputFileNames(char*);
static int CalculateInputImageHash(uint64_t*, uint64_t*);
/*
 * Info file
 */
static int InitInfoFile();
/*
 * Lib related
 */
static int LoadLibs();
static int FindInputLib(pts_InputImage);
static int FindMorphingLib();
static int FindOutputLib();
static int SplitLibraryParameters(char*, uint32_t*, pts_LibXmountOptions**);
/*
 * Functions exported to LibXmount_Morphing
 */
static int LibXmount_Morphing_ImageCount(uint64_t*);
static int LibXmount_Morphing_Size(uint64_t, uint64_t*);
static int LibXmount_Morphing_Read(uint64_t, char*, off_t, size_t, size_t*);
/*
 * Functions exported to LibXmount_Output
 */
static int LibXmount_Output_Size(uint64_t*);
static int LibXmount_Output_Read(char*, off_t, size_t, size_t*);
static int LibXmount_Output_Write(char*, off_t, size_t, size_t*);

/*******************************************************************************
 * Helper functions
 ******************************************************************************/
//! Print usage instructions (cmdline options etc..)
/*!
 * \param p_prog_name Program name (argv[0])
 */
static void PrintUsage(char *p_prog_name) {
  char *p_buf;
  int first;
  int ret;

  printf("\n" XMOUNT_COPYRIGHT_NOTICE "\n",XMOUNT_VERSION);
  printf("\nUsage:\n");
  printf("  %s [fopts] <xopts> <mntp>\n\n",p_prog_name);
  printf("Options:\n");
  printf("  fopts:\n");
  printf("    -d : Enable FUSE's and xmount's debug mode.\n");
  printf("    -h : Display this help message.\n");
  printf("    -s : Run single threaded.\n");
  printf("    -o no_allow_other : Disable automatic addition of FUSE's "
           "allow_other option.\n");
  printf("    -o <fopts> : Specify fuse mount options. Will also disable "
           "automatic addition of FUSE's allow_other option!\n");
  printf("\n");
  printf("  xopts:\n");
  printf("    --cache <cfile> : Enable virtual write support.\n");
  printf("      <cfile> specifies the cache file to use.\n");
  printf("    --in <itype> <ifile> : Input image format and source file(s). "
           "May be specified multiple times.\n");
  printf("      <itype> can be ");

  // List supported input formats
  first=1;
  for(uint32_t i=0;i<glob_xmount.input.libs_count;i++) {
    p_buf=glob_xmount.input.pp_libs[i]->p_supported_input_types;
    while(*p_buf!='\0') {
      if(first==1) {
        printf("\"%s\"",p_buf);
        first=0;
      } else printf(", \"%s\"",p_buf);
      p_buf+=(strlen(p_buf)+1);
    }
  }
  printf(".\n");

  printf("      <ifile> specifies the source file. If your image is split into "
           "multiple files, you have to specify them all!\n");
  printf("    --inopts <iopts> : Specify input library specific options.\n");
  printf("      <iopts> specifies a comma separated list of key=value options. "
           "See below for details.\n");
  printf("    --info : Print out infos about used compiler and libraries.\n");
  printf("    --morph <mtype> : Morphing function to apply to input image(s). "
           "If not specified, defaults to \"combine\".\n");
  printf("      <mtype> can be ");

  // List supported morphing functions
  first=1;
  for(uint32_t i=0;i<glob_xmount.morphing.libs_count;i++) {
    p_buf=glob_xmount.morphing.pp_libs[i]->p_supported_morphing_types;
    while(*p_buf!='\0') {
      if(first==1) {
        printf("\"%s\"",p_buf);
        first=0;
      } else printf(", \"%s\"",p_buf);
      p_buf+=(strlen(p_buf)+1);
    }
  }
  printf(".\n");

  printf("    --morphopts <mopts> : Specify morphing library specific "
           "options.\n");
  printf("      <mopts> specifies a comma separated list of key=value options. "
           "See below for details.\n");
  printf("    --offset <off> : Move the output image data start <off> bytes "
           "into the input image(s).\n");
  printf("    --out <otype> : Output image format. If not specified, "
           "defaults to ");
#ifdef __APPLE__
  printf("\"dmg\".\n");
#else
  printf("\"raw\".\n");
#endif
  printf("      <otype> can be ");

  // List supported output formats
  first=1;
  for(uint32_t i=0;i<glob_xmount.output.libs_count;i++) {
    p_buf=glob_xmount.output.pp_libs[i]->p_supported_output_formats;
    while(*p_buf!='\0') {
      if(first==1) {
        printf("\"%s\"",p_buf);
        first=0;
      } else printf(", \"%s\"",p_buf);
      p_buf+=(strlen(p_buf)+1);
    }
  }
  printf(".\n");

  printf("    --outopts <oopts> : Specify output library specific "
           "options.\n");
  printf("    --owcache <file> : Same as --cache <file> but overwrites "
           "existing cache file.\n");
  printf("    --sizelimit <size> : The data end of input image(s) is set to no "
           "more than <size> bytes after the data start.\n");
  printf("    --version : Same as --info.\n");
  printf("\n");
  printf("  mntp:\n");
  printf("    Mount point where output image should be located.\n");
  printf("\n");
  printf("Infos:\n");
  printf("  * One --in option and a mount point are mandatory!\n");
  printf("  * If you specify --in multiple times, data from all images is "
           "morphed into one output image using the specified morphing "
           "function.\n");
  printf("  * For VMDK emulation, you have to uncomment \"user_allow_other\" "
           "in /etc/fuse.conf or run xmount as root.\n");
  printf("\n");
  printf("Input / Morphing library specific options:\n");
  printf("  Input / Morphing libraries might support an own set of "
           "options to configure / tune their behaviour.\n");
  printf("  Libraries supporting this feature (if any) and their "
           "options are listed below.\n");
  printf("\n");

  // List input, morphing and output lib options
  for(uint32_t i=0;i<glob_xmount.input.libs_count;i++) {
    ret=glob_xmount.input.pp_libs[i]->
          lib_functions.OptionsHelp((const char**)&p_buf);
    if(ret!=0) {
      LOG_ERROR("Unable to get options help for library '%s': %s!\n",
                glob_xmount.input.pp_libs[i]->p_name,
                glob_xmount.input.pp_libs[i]->
                  lib_functions.GetErrorMessage(ret));
    }
    if(p_buf==NULL) continue;
    printf("  - %s\n",glob_xmount.input.pp_libs[i]->p_name);
    printf("%s",p_buf);
    printf("\n");
    ret=glob_xmount.input.pp_libs[i]->lib_functions.FreeBuffer(p_buf);
    if(ret!=0) {
      LOG_ERROR("Unable to free options help text from library '%s': %s!\n",
                glob_xmount.input.pp_libs[i]->p_name,
                glob_xmount.input.pp_libs[i]->
                  lib_functions.GetErrorMessage(ret));
    }
  }
  for(uint32_t i=0;i<glob_xmount.morphing.libs_count;i++) {
    ret=glob_xmount.morphing.pp_libs[i]->
          lib_functions.OptionsHelp((const char**)&p_buf);
    if(ret!=0) {
      LOG_ERROR("Unable to get options help for library '%s': %s!\n",
                glob_xmount.morphing.pp_libs[i]->p_name,
                glob_xmount.morphing.pp_libs[i]->
                  lib_functions.GetErrorMessage(ret));
    }
    if(p_buf==NULL) continue;
    printf("  - %s\n",glob_xmount.morphing.pp_libs[i]->p_name);
    printf("%s",p_buf);
    printf("\n");
  }
  for(uint32_t i=0;i<glob_xmount.output.libs_count;i++) {
    ret=glob_xmount.output.pp_libs[i]->
          lib_functions.OptionsHelp((const char**)&p_buf);
    if(ret!=0) {
      LOG_ERROR("Unable to get options help for library '%s': %s!\n",
                glob_xmount.output.pp_libs[i]->p_name,
                glob_xmount.output.pp_libs[i]->
                  lib_functions.GetErrorMessage(ret));
    }
    if(p_buf==NULL) continue;
    printf("  - %s\n",glob_xmount.output.pp_libs[i]->p_name);
    printf("%s",p_buf);
    printf("\n");
  }
}

//! Check fuse settings
/*!
 * Check if FUSE allows us to pass the -o allow_other parameter. This only works
 * if we are root or user_allow_other is set in /etc/fuse.conf.
 *
 * In addition, this function also checks if the user is member of the fuse
 * group which is generally needed to use fuse at all.
 */
static void CheckFuseSettings() {
#if !defined(__APPLE__) && defined(HAVE_GRP_H) && defined(HAVE_PWD_H)
  struct group *p_group;
  struct passwd *p_passwd;
#endif
  int found;
  FILE *h_fuse_conf;
  char line[256];

  glob_xmount.may_set_fuse_allow_other=FALSE;

  if(geteuid()==0) {
    // Running as root, there should be no problems
    glob_xmount.may_set_fuse_allow_other=TRUE;
    return;
  }

#if !defined(__APPLE__) && defined(HAVE_GRP_H) && defined(HAVE_PWD_H)
  // Check if a fuse group exists and if so, make sure user is a member of it.
  // Makes only sense on Linux because as far as I know osxfuse has no own group
  p_group=getgrnam("fuse");
  if(p_group!=NULL) {
    // Get effective user name
    p_passwd=getpwuid(geteuid());
    if(p_passwd==NULL) {
      printf("\nWARNING: Unable to determine your effective user name. If "
             "mounting works, you can ignore this message.\n\n");
      return;
    }
    // Check if user is member of fuse group
    found=FALSE;
    while(*(p_group->gr_mem)!=NULL) {
      if(strcmp(*(p_group->gr_mem),p_passwd->pw_name)==0) {
        found=TRUE;
        break;
      }
      p_group->gr_mem++;
    }
    if(found==FALSE) {
      printf("\nWARNING: You are not a member of the \"fuse\" group. This will "
               "prevent you from mounting images using xmount. Please add "
               "yourself to the \"fuse\" group using the command "
               "\"sudo usermod -a -G fuse %s\" and reboot your system or "
               "execute xmount as root.\n\n",
             p_passwd->pw_name);
      return;
    }
  } else {
    printf("\nWARNING: Your system does not seem to have a \"fuse\" group. If "
             "mounting works, you can ignore this message.\n\n");
  }
#endif

  // Read FUSE's config file /etc/fuse.conf and check for set user_allow_other
  h_fuse_conf=(FILE*)FOPEN("/etc/fuse.conf","r");
  if(h_fuse_conf!=NULL) {
    // Search conf file for set user_allow_others
    found=FALSE;
    while(fgets(line,sizeof(line),h_fuse_conf)!=NULL) {
      // TODO: This works as long as there is no other parameter beginning with
      // "user_allow_other" :)
      if(strncmp(line,"user_allow_other",16)==0) {
        found=TRUE;
        break;
      }
    }
    fclose(h_fuse_conf);
    if(found==TRUE) {
      glob_xmount.may_set_fuse_allow_other=TRUE;
    } else {
      printf("\nWARNING: FUSE will not allow other users nor root to access "
               "your virtual harddisk image. To change this behavior, please "
               "add \"user_allow_other\" to /etc/fuse.conf or execute xmount "
               "as root.\n\n");
    }
  } else {
    printf("\nWARNING: Unable to open /etc/fuse.conf. If mounting works, you "
             "can ignore this message. If you encounter issues, please create "
             "the file and add a single line containing the string "
             "\"user_allow_other\" or execute xmount as root.\n\n");
    return;
  }
}

//! Parse command line options
/*!
 * \param argc Number of cmdline params
 * \param pp_argv Array containing cmdline params
 * \return TRUE on success, FALSE on error
 */
static int ParseCmdLine(const int argc, char **pp_argv) {
  int i=1;
  int FuseMinusOControl=TRUE;
  int FuseAllowOther=TRUE;
  int first;
  char *p_buf;
  pts_InputImage p_input_image=NULL;
  int ret;

  // add pp_argv[0] to FUSE's argv
  XMOUNT_MALLOC(glob_xmount.pp_fuse_argv,char**,sizeof(char*));
  XMOUNT_STRSET(glob_xmount.pp_fuse_argv[0],pp_argv[0]);
  glob_xmount.fuse_argc=1;

  // Parse options
  while(i<argc && *pp_argv[i]=='-') {
    if(strlen(pp_argv[i])>1 && *(pp_argv[i]+1)!='-') {
      // Options beginning with one - are mostly FUSE specific
      if(strcmp(pp_argv[i],"-d")==0) {
        // Enable FUSE's and xmount's debug mode
        XMOUNT_REALLOC(glob_xmount.pp_fuse_argv,
                       char**,
                       (glob_xmount.fuse_argc+1)*sizeof(char*));
        XMOUNT_STRSET(glob_xmount.pp_fuse_argv[glob_xmount.fuse_argc],
                      pp_argv[i])
        glob_xmount.fuse_argc++;
        glob_xmount.debug=TRUE;
      } else if(strcmp(pp_argv[i],"-h")==0) {
        // Print help message
        PrintUsage(pp_argv[0]);
        exit(0);
      } else if(strcmp(pp_argv[i],"-o")==0) {
        // Next parameter specifies fuse mount options
        if((i+1)<argc) {
          i++;
          // As the user specified the -o option, we assume he knows what he is
          // doing. We won't append allow_other automatically. And we allow him
          // to disable allow_other by passing a single "-o no_allow_other"
          // which won't be passed to FUSE as it is xmount specific.
          if(strcmp(pp_argv[i],"no_allow_other")!=0) {
            glob_xmount.fuse_argc+=2;
            XMOUNT_REALLOC(glob_xmount.pp_fuse_argv,
                           char**,
                           glob_xmount.fuse_argc*sizeof(char*));
            XMOUNT_STRSET(glob_xmount.pp_fuse_argv[glob_xmount.fuse_argc-2],
                          pp_argv[i-1]);
            XMOUNT_STRSET(glob_xmount.pp_fuse_argv[glob_xmount.fuse_argc-1],
                          pp_argv[i]);
            FuseMinusOControl=FALSE;
          } else FuseAllowOther=FALSE;
        } else {
          LOG_ERROR("Couldn't parse fuse mount options!\n")
          return FALSE;
        }
      } else if(strcmp(pp_argv[i],"-s")==0) {
        // Enable FUSE's single threaded mode
        XMOUNT_REALLOC(glob_xmount.pp_fuse_argv,
                       char**,
                       (glob_xmount.fuse_argc+1)*sizeof(char*));
        XMOUNT_STRSET(glob_xmount.pp_fuse_argv[glob_xmount.fuse_argc],
                      pp_argv[i]);
        glob_xmount.fuse_argc++;
      } else if(strcmp(pp_argv[i],"-V")==0) {
        // Display FUSE version info
        XMOUNT_REALLOC(glob_xmount.pp_fuse_argv,
                       char**,
                       (glob_xmount.fuse_argc+1)*sizeof(char*));
        XMOUNT_STRSET(glob_xmount.pp_fuse_argv[glob_xmount.fuse_argc],
                      pp_argv[i]);
        glob_xmount.fuse_argc++;
      } else {
        LOG_ERROR("Unknown command line option \"%s\"\n",pp_argv[i]);
        return FALSE;
      }
    } else {
      // Options beginning with -- are xmount specific
      if(strcmp(pp_argv[i],"--cache")==0 /*|| strcmp(pp_argv[i],"--rw")==0*/) {
        // Emulate writable access to mounted image
        // Next parameter must be cache file to read/write changes from/to
        if((i+1)<argc) {
          i++;
          XMOUNT_STRSET(glob_xmount.args.p_cache_file,pp_argv[i])
          glob_xmount.output.writable=TRUE;
        } else {
          LOG_ERROR("You must specify a cache file!\n")
          return FALSE;
        }
        LOG_DEBUG("Enabling virtual write support using cache file \"%s\"\n",
                  glob_xmount.args.p_cache_file)
      } else if(strcmp(pp_argv[i],"--in")==0) {
        // Specify input image type and source files
        if((i+2)<argc) {
          i++;
          // Alloc and init new ts_InputImage struct
          XMOUNT_MALLOC(p_input_image,pts_InputImage,sizeof(ts_InputImage));
          XMOUNT_STRSET(p_input_image->p_type,pp_argv[i]);
          p_input_image->pp_files=NULL;
          p_input_image->p_functions=NULL;
          p_input_image->p_handle=NULL;
          // Parse input image filename(s) and add to p_input_image->pp_files
          i++;
          p_input_image->files_count=0;
          while(i<(argc-1) && strncmp(pp_argv[i],"--",2)!=0) {
            p_input_image->files_count++;
            XMOUNT_REALLOC(p_input_image->pp_files,
                           char**,
                           p_input_image->files_count*sizeof(char*));
            XMOUNT_STRSET(p_input_image->pp_files[p_input_image->files_count-1],
                          pp_argv[i]);
            i++;
          }
          i--;
          if(p_input_image->files_count==0) {
            LOG_ERROR("No input files specified for \"--in %s\"!\n",
                      p_input_image->p_type)
            free(p_input_image->p_type);
            free(p_input_image);
            return FALSE;
          }
          // Add input image struct to input image array
          glob_xmount.input.images_count++;
          XMOUNT_REALLOC(glob_xmount.input.pp_images,
                         pts_InputImage*,
                         glob_xmount.input.images_count*
                           sizeof(pts_InputImage));
          glob_xmount.input.pp_images[glob_xmount.input.images_count-1]=
            p_input_image;
        } else {
          LOG_ERROR("You must specify an input image type and source file!\n");
          return FALSE;
        }
      } else if(strcmp(pp_argv[i],"--inopts")==0) {
        // Set input lib options
        if((i+1)<argc) {
          i++;
          if(glob_xmount.input.pp_lib_params==NULL) {
            if(SplitLibraryParameters(pp_argv[i],
                                      &(glob_xmount.input.lib_params_count),
                                      &(glob_xmount.input.pp_lib_params)
                                     )==FALSE)
            {
              LOG_ERROR("Unable to parse input library options '%s'!\n",
                        pp_argv[i]);
              return FALSE;
            }
          } else {
            LOG_ERROR("You can only specify --inopts once!")
            return FALSE;
          }
        } else {
          LOG_ERROR("You must specify special options!\n");
          return FALSE;
        }
      } else if(strcmp(pp_argv[i],"--morph")==0) {
        // Set morphing lib to use
        if((i+1)<argc) {
          i++;
          if(glob_xmount.morphing.p_morph_type==NULL) {
            XMOUNT_STRSET(glob_xmount.morphing.p_morph_type,pp_argv[i]);
          } else {
            LOG_ERROR("You can only specify --morph once!")
            return FALSE;
          }
        } else {
          LOG_ERROR("You must specify morphing type!\n");
          return FALSE;
        }
      } else if(strcmp(pp_argv[i],"--morphopts")==0) {
        // Set morphing lib options
        if((i+1)<argc) {
          i++;
          if(glob_xmount.morphing.pp_lib_params==NULL) {
            if(SplitLibraryParameters(pp_argv[i],
                                      &(glob_xmount.morphing.lib_params_count),
                                      &(glob_xmount.morphing.pp_lib_params)
                                     )==FALSE)
            {
              LOG_ERROR("Unable to parse morphing library options '%s'!\n",
                        pp_argv[i]);
              return FALSE;
            }
          } else {
            LOG_ERROR("You can only specify --morphopts once!")
            return FALSE;
          }
        } else {
          LOG_ERROR("You must specify special morphing lib params!\n");
          return FALSE;
        }
      } else if(strcmp(pp_argv[i],"--offset")==0) {
        // Set input image offset
        if((i+1)<argc) {
          i++;
          glob_xmount.input.image_offset=StrToUint64(pp_argv[i],&ret);
          if(ret==0) {
            LOG_ERROR("Unable to convert '%s' to a number!\n",pp_argv[i])
            return FALSE;
          }
        } else {
          LOG_ERROR("You must specify an offset!\n")
          return FALSE;
        }
        LOG_DEBUG("Setting input image offset to \"%" PRIu64 "\"\n",
                  glob_xmount.input.image_offset)
      } else if(strcmp(pp_argv[i],"--out")==0) {
        // Set output lib to use
        if((i+1)<argc) {
          i++;
          if(glob_xmount.output.p_output_format==NULL) {
            XMOUNT_STRSET(glob_xmount.output.p_output_format,pp_argv[i]);
          } else {
            LOG_ERROR("You can only specify --out once!")
            return FALSE;
          }
        } else {
          LOG_ERROR("You must specify an output format!\n");
          return FALSE;
        }
      } else if(strcmp(pp_argv[i],"--outopts")==0) {
        // Set output lib options
        if((i+1)<argc) {
          i++;
          if(glob_xmount.output.pp_lib_params==NULL) {
            if(SplitLibraryParameters(pp_argv[i],
                                      &(glob_xmount.output.lib_params_count),
                                      &(glob_xmount.output.pp_lib_params)
                                     )==FALSE)
            {
              LOG_ERROR("Unable to parse output library options '%s'!\n",
                        pp_argv[i]);
              return FALSE;
            }
          } else {
            LOG_ERROR("You can only specify --outopts once!")
            return FALSE;
          }
        } else {
          LOG_ERROR("You must specify special output lib params!\n");
          return FALSE;
        }
      } else if(strcmp(pp_argv[i],"--owcache")==0) {
        // Enable writable access to mounted image and overwrite existing cache
        // Next parameter must be cache file to read/write changes from/to
        if((i+1)<argc) {
          i++;
          XMOUNT_STRSET(glob_xmount.args.p_cache_file,pp_argv[i])
          glob_xmount.output.writable=TRUE;
          glob_xmount.args.overwrite_cache=TRUE;
        } else {
          LOG_ERROR("You must specify a cache file!\n")
          return FALSE;
        }
        LOG_DEBUG("Enabling virtual write support overwriting cache file %s\n",
                  glob_xmount.args.p_cache_file)
      } else if(strcmp(pp_argv[i],"--sizelimit")==0) {
        // Set input image size limit
        if((i+1)<argc) {
          i++;
          glob_xmount.input.image_size_limit=StrToUint64(pp_argv[i],&ret);
          if(ret==0) {
            LOG_ERROR("Unable to convert '%s' to a number!\n",pp_argv[i])
            return FALSE;
          }
        } else {
          LOG_ERROR("You must specify a size limit!\n")
          return FALSE;
        }
        LOG_DEBUG("Setting input image size limit to \"%" PRIu64 "\"\n",
                  glob_xmount.input.image_size_limit)
      } else if(strcmp(pp_argv[i],"--version")==0 ||
                strcmp(pp_argv[i],"--info")==0)
      {
        // Print xmount info
        printf(XMOUNT_COPYRIGHT_NOTICE "\n\n",XMOUNT_VERSION);
#ifdef __GNUC__
        printf("  compile timestamp: %s %s\n",__DATE__,__TIME__);
        printf("  gcc version: %s\n",__VERSION__);
#endif
        printf("  loaded input libraries:\n");
        for(uint32_t ii=0;ii<glob_xmount.input.libs_count;ii++) {
          printf("    - %s supporting ",glob_xmount.input.pp_libs[ii]->p_name);
          p_buf=glob_xmount.input.pp_libs[ii]->p_supported_input_types;
          first=TRUE;
          while(*p_buf!='\0') {
            if(first) {
              printf("\"%s\"",p_buf);
              first=FALSE;
            } else printf(", \"%s\"",p_buf);
            p_buf+=(strlen(p_buf)+1);
          }
          printf("\n");
        }
        printf("  loaded morphing libraries:\n");
        for(uint32_t ii=0;ii<glob_xmount.morphing.libs_count;ii++) {
          printf("    - %s supporting ",
                 glob_xmount.morphing.pp_libs[ii]->p_name);
          p_buf=glob_xmount.morphing.pp_libs[ii]->p_supported_morphing_types;
          first=TRUE;
          while(*p_buf!='\0') {
            if(first) {
              printf("\"%s\"",p_buf);
              first=FALSE;
            } else printf(", \"%s\"",p_buf);
            p_buf+=(strlen(p_buf)+1);
          }
          printf("\n");
        }
        printf("  loaded output libraries:\n");
        for(uint32_t ii=0;ii<glob_xmount.output.libs_count;ii++) {
          printf("    - %s supporting ",
                 glob_xmount.output.pp_libs[ii]->p_name);
          p_buf=glob_xmount.output.pp_libs[ii]->p_supported_output_formats;
          first=TRUE;
          while(*p_buf!='\0') {
            if(first) {
              printf("\"%s\"",p_buf);
              first=FALSE;
            } else printf(", \"%s\"",p_buf);
            p_buf+=(strlen(p_buf)+1);
          }
          printf("\n");
        }
        printf("\n");
        exit(0);
      } else {
        LOG_ERROR("Unknown command line option \"%s\"\n",pp_argv[i]);
        return FALSE;
      }
    }
    i++;
  }

  // Extract mountpoint
  if(i==(argc-1)) {
    XMOUNT_STRSET(glob_xmount.p_mountpoint,pp_argv[argc-1])
    XMOUNT_REALLOC(glob_xmount.pp_fuse_argv,
                   char**,
                   (glob_xmount.fuse_argc+1)*sizeof(char*));
    XMOUNT_STRSET(glob_xmount.pp_fuse_argv[glob_xmount.fuse_argc],
                  glob_xmount.p_mountpoint);
    glob_xmount.fuse_argc++;
  } else {
    LOG_ERROR("No mountpoint specified!\n")
    return FALSE;
  }

  if(FuseMinusOControl==TRUE) {
    // We control the -o flag, set subtype, fsname and allow_other options
    glob_xmount.fuse_argc+=2;
    XMOUNT_REALLOC(glob_xmount.pp_fuse_argv,
                   char**,
                   glob_xmount.fuse_argc*sizeof(char*));
    XMOUNT_STRSET(glob_xmount.pp_fuse_argv[glob_xmount.fuse_argc-2],"-o");
    XMOUNT_STRSET(glob_xmount.pp_fuse_argv[glob_xmount.fuse_argc-1],
                  "subtype=xmount");
    if(glob_xmount.input.images_count!=0) {
      // Set name of first source file as fsname
      XMOUNT_STRAPP(glob_xmount.pp_fuse_argv[glob_xmount.fuse_argc-1],
                    ",fsname='");
      // If possible, use full path
      p_buf=realpath(glob_xmount.input.pp_images[0]->pp_files[0],NULL);
      if(p_buf==NULL) {
        XMOUNT_STRSET(p_buf,glob_xmount.input.pp_images[0]->pp_files[0]);
      }
      // Make sure fsname does not include some forbidden chars
      for(uint32_t i=0;i<strlen(p_buf);i++) {
        if(p_buf[i]=='\'') p_buf[i]='_';
      }
      // Set fsname
      XMOUNT_STRAPP(glob_xmount.pp_fuse_argv[glob_xmount.fuse_argc-1],
                    p_buf);
      XMOUNT_STRAPP(glob_xmount.pp_fuse_argv[glob_xmount.fuse_argc-1],
                    "'");
      free(p_buf);
    }
    if(FuseAllowOther==TRUE) {
      // Add "allow_other" option if allowed
      if(glob_xmount.may_set_fuse_allow_other) {
        XMOUNT_STRAPP(glob_xmount.pp_fuse_argv[glob_xmount.fuse_argc-1],
                      ",allow_other");
      }
    }
  }

  return TRUE;
}

//! Extract output file name from input image name
/*!
 * \param p_orig_name Name of input image (may include a path)
 * \return TRUE on success, FALSE on error
 */
static int ExtractOutputFileNames(char *p_orig_name) {
  char *tmp;

  // Truncate any leading path
  tmp=strrchr(p_orig_name,'/');
  if(tmp!=NULL) p_orig_name=tmp+1;

  // Extract file extension
  tmp=strrchr(p_orig_name,'.');

  // Set leading '/'
  XMOUNT_STRSET(glob_xmount.output.p_virtual_image_path,"/");
  XMOUNT_STRSET(glob_xmount.output.p_info_path,"/");

  // Copy filename
  if(tmp==NULL) {
    // Input image filename has no extension
    XMOUNT_STRAPP(glob_xmount.output.p_virtual_image_path,p_orig_name);
    XMOUNT_STRAPP(glob_xmount.output.p_info_path,p_orig_name);
    XMOUNT_STRAPP(glob_xmount.output.p_info_path,".info");
  } else {
    XMOUNT_STRNAPP(glob_xmount.output.p_virtual_image_path,p_orig_name,
                   strlen(p_orig_name)-strlen(tmp));
    XMOUNT_STRNAPP(glob_xmount.output.p_info_path,p_orig_name,
                   strlen(p_orig_name)-strlen(tmp));
    XMOUNT_STRAPP(glob_xmount.output.p_info_path,".info");
  }

  // Add virtual file extensions
  // TODO: Get from output lib and add
  //XMOUNT_STRAPP(glob_xmount.output.p_virtual_image_path,".dd");

  LOG_DEBUG("Set virtual image name to \"%s\"\n",
            glob_xmount.output.p_virtual_image_path);
  LOG_DEBUG("Set virtual image info name to \"%s\"\n",
            glob_xmount.output.p_info_path);
  return TRUE;
}

//! Calculates an MD5 hash of the first HASH_AMOUNT bytes of the input image
/*!
 * \param p_hash_low Pointer to the lower 64 bit of the hash
 * \param p_hash_high Pointer to the higher 64 bit of the hash
 * \return TRUE on success, FALSE on error
 */
static int CalculateInputImageHash(uint64_t *p_hash_low,
                                   uint64_t *p_hash_high)
{
  char hash[16];
  md5_state_t md5_state;
  char *p_buf;
  int ret;
  size_t read_data;

  XMOUNT_MALLOC(p_buf,char*,HASH_AMOUNT*sizeof(char));
  ret=ReadMorphedImageData(p_buf,0,HASH_AMOUNT,&read_data);
  if(ret!=TRUE || read_data==0) {
    LOG_ERROR("Couldn't read data from morphed image file!\n")
    free(p_buf);
    return FALSE;
  }

  // Calculate MD5 hash
  md5_init(&md5_state);
  md5_append(&md5_state,(const md5_byte_t*)p_buf,read_data);
  md5_finish(&md5_state,(md5_byte_t*)hash);
  // Convert MD5 hash into two 64bit integers
  *p_hash_low=*((uint64_t*)hash);
  *p_hash_high=*((uint64_t*)(hash+8));
  free(p_buf);

  return TRUE;
}

//! Create info file
/*!
 * \return TRUE on success, FALSE on error
 */
static int InitInfoFile() {
  int ret;
  char *p_buf;

  // Start with static input header
  XMOUNT_MALLOC(glob_xmount.output.p_info_file,
                char*,
                strlen(IMAGE_INFO_INPUT_HEADER)+1);
  strncpy(glob_xmount.output.p_info_file,
          IMAGE_INFO_INPUT_HEADER,
          strlen(IMAGE_INFO_INPUT_HEADER)+1);

  // Get and add infos from input lib(s)
  for(uint64_t i=0;i<glob_xmount.input.images_count;i++) {
    ret=glob_xmount.input.pp_images[i]->p_functions->
          GetInfofileContent(glob_xmount.input.pp_images[i]->p_handle,(const char**)&p_buf);
    if(ret!=0) {
      LOG_ERROR("Unable to get info file content for image '%s': %s!\n",
                glob_xmount.input.pp_images[i]->pp_files[0],
                glob_xmount.input.pp_images[i]->p_functions->
                  GetErrorMessage(ret));
      return FALSE;
    }
    // Add infos to main buffer and free p_buf
    XMOUNT_STRAPP(glob_xmount.output.p_info_file,"\n--> ");
    XMOUNT_STRAPP(glob_xmount.output.p_info_file,
                  glob_xmount.input.pp_images[i]->pp_files[0]);
    XMOUNT_STRAPP(glob_xmount.output.p_info_file," <--\n");
    if(p_buf!=NULL) {
      XMOUNT_STRAPP(glob_xmount.output.p_info_file,p_buf);
      glob_xmount.input.pp_images[i]->p_functions->FreeBuffer(p_buf);
    } else {
      XMOUNT_STRAPP(glob_xmount.output.p_info_file,"None\n");
    }
  }

  // Add static morphing header
  XMOUNT_STRAPP(glob_xmount.output.p_info_file,IMAGE_INFO_MORPHING_HEADER);

  // Get and add infos from morphing lib
  ret=glob_xmount.morphing.p_functions->
        GetInfofileContent(glob_xmount.morphing.p_handle,(const char**)&p_buf);
  if(ret!=0) {
    LOG_ERROR("Unable to get info file content from morphing lib: %s!\n",
              glob_xmount.morphing.p_functions->GetErrorMessage(ret));
    return FALSE;
  }
  if(p_buf!=NULL) {
    XMOUNT_STRAPP(glob_xmount.output.p_info_file,p_buf);
    glob_xmount.morphing.p_functions->FreeBuffer(p_buf);
  } else {
    XMOUNT_STRAPP(glob_xmount.output.p_info_file,"None\n");
  }

  return TRUE;
}

//! Load input / morphing libs
/*!
 * \return TRUE on success, FALSE on error
 */
static int LoadLibs() {
  DIR *p_dir=NULL;
  struct dirent *p_dirent=NULL;
  int base_library_path_len=0;
  char *p_library_path=NULL;
  void *p_libxmount=NULL;
  t_LibXmount_Input_GetApiVersion pfun_input_GetApiVersion;
  t_LibXmount_Input_GetSupportedFormats pfun_input_GetSupportedFormats;
  t_LibXmount_Input_GetFunctions pfun_input_GetFunctions;
  t_LibXmount_Morphing_GetApiVersion pfun_morphing_GetApiVersion;
  t_LibXmount_Morphing_GetSupportedTypes pfun_morphing_GetSupportedTypes;
  t_LibXmount_Morphing_GetFunctions pfun_morphing_GetFunctions;
  t_LibXmount_Output_GetApiVersion pfun_output_GetApiVersion;
  t_LibXmount_Output_GetSupportedFormats pfun_output_GetSupportedFormats;
  t_LibXmount_Output_GetFunctions pfun_output_GetFunctions;
  const char *p_supported_formats=NULL;
  const char *p_buf;
  uint32_t supported_formats_len=0;
  pts_InputLib p_input_lib=NULL;
  pts_MorphingLib p_morphing_lib=NULL;
  pts_OutputLib p_output_lib=NULL;

  LOG_DEBUG("Searching for xmount libraries in '%s'.\n",
            XMOUNT_LIBRARY_PATH);

  // Open lib dir
  p_dir=opendir(XMOUNT_LIBRARY_PATH);
  if(p_dir==NULL) {
    LOG_ERROR("Unable to access xmount library directory '%s'!\n",
              XMOUNT_LIBRARY_PATH);
    return FALSE;
  }

  // Construct base library path
  base_library_path_len=strlen(XMOUNT_LIBRARY_PATH);
  XMOUNT_STRSET(p_library_path,XMOUNT_LIBRARY_PATH);
  if(XMOUNT_LIBRARY_PATH[base_library_path_len]!='/') {
    base_library_path_len++;
    XMOUNT_STRAPP(p_library_path,"/");
  }

#define LIBXMOUNT_LOAD(path) {                            \
  p_libxmount=dlopen(path,RTLD_NOW);                      \
  if(p_libxmount==NULL) {                                 \
    LOG_ERROR("Unable to load input library '%s': %s!\n", \
              path,                                       \
              dlerror());                                 \
    continue;                                             \
  }                                                       \
}
#define LIBXMOUNT_LOAD_SYMBOL(name,pfun) {                       \
  if((pfun=dlsym(p_libxmount,name))==NULL) {                     \
    LOG_ERROR("Unable to load symbol '%s' from library '%s'!\n", \
              name,                                              \
              p_library_path);                                   \
    dlclose(p_libxmount);                                        \
    p_libxmount=NULL;                                            \
    continue;                                                    \
  }                                                              \
}

  // Loop over lib dir
  while((p_dirent=readdir(p_dir))!=NULL) {
    LOG_DEBUG("Trying to load '%s'\n",p_dirent->d_name);

    // Construct full path to found object
    p_library_path=realloc(p_library_path,
                           base_library_path_len+strlen(p_dirent->d_name)+1);
    if(p_library_path==NULL) {
      LOG_ERROR("Couldn't allocate memory!\n");
      exit(1);
    }
    strcpy(p_library_path+base_library_path_len,p_dirent->d_name);

    if(strncmp(p_dirent->d_name,"libxmount_input_",16)==0) {
      // Found possible input lib. Try to load it
      LIBXMOUNT_LOAD(p_library_path);

      // Load library symbols
      LIBXMOUNT_LOAD_SYMBOL("LibXmount_Input_GetApiVersion",
                            pfun_input_GetApiVersion);

      // Check library's API version
      if(pfun_input_GetApiVersion()!=LIBXMOUNT_INPUT_API_VERSION) {
        LOG_DEBUG("Failed! Wrong API version.\n");
        LOG_ERROR("Unable to load input library '%s'. Wrong API version\n",
                  p_library_path);
        dlclose(p_libxmount);
        continue;
      }

      LIBXMOUNT_LOAD_SYMBOL("LibXmount_Input_GetSupportedFormats",
                            pfun_input_GetSupportedFormats);
      LIBXMOUNT_LOAD_SYMBOL("LibXmount_Input_GetFunctions",
                            pfun_input_GetFunctions);

      // Construct new entry for our library list
      XMOUNT_MALLOC(p_input_lib,pts_InputLib,sizeof(ts_InputLib));
      // Initialize lib_functions structure to NULL
      memset(&(p_input_lib->lib_functions),
             0,
             sizeof(ts_LibXmountInputFunctions));

      // Set name and handle
      XMOUNT_STRSET(p_input_lib->p_name,p_dirent->d_name);
      p_input_lib->p_lib=p_libxmount;

      // Get and set supported formats
      p_supported_formats=pfun_input_GetSupportedFormats();
      supported_formats_len=0;
      p_buf=p_supported_formats;
      while(*p_buf!='\0') {
        supported_formats_len+=(strlen(p_buf)+1);
        p_buf+=(strlen(p_buf)+1);
      }
      supported_formats_len++;
      XMOUNT_MALLOC(p_input_lib->p_supported_input_types,
                    char*,
                    supported_formats_len);
      memcpy(p_input_lib->p_supported_input_types,
             p_supported_formats,
             supported_formats_len);

      // Get, set and check lib_functions
      pfun_input_GetFunctions(&(p_input_lib->lib_functions));
      if(p_input_lib->lib_functions.CreateHandle==NULL ||
         p_input_lib->lib_functions.DestroyHandle==NULL ||
         p_input_lib->lib_functions.Open==NULL ||
         p_input_lib->lib_functions.Close==NULL ||
         p_input_lib->lib_functions.Size==NULL ||
         p_input_lib->lib_functions.Read==NULL ||
         p_input_lib->lib_functions.OptionsHelp==NULL ||
         p_input_lib->lib_functions.OptionsParse==NULL ||
         p_input_lib->lib_functions.GetInfofileContent==NULL ||
         p_input_lib->lib_functions.GetErrorMessage==NULL ||
         p_input_lib->lib_functions.FreeBuffer==NULL)
      {
        LOG_DEBUG("Missing implemention of one or more functions in lib %s!\n",
                  p_dirent->d_name);
        free(p_input_lib->p_supported_input_types);
        free(p_input_lib->p_name);
        free(p_input_lib);
        dlclose(p_libxmount);
        continue;
      }

      // Add entry to the input library list
      XMOUNT_REALLOC(glob_xmount.input.pp_libs,
                     pts_InputLib*,
                     sizeof(pts_InputLib)*(glob_xmount.input.libs_count+1));
      glob_xmount.input.pp_libs[glob_xmount.input.libs_count++]=p_input_lib;

      LOG_DEBUG("Input library '%s' loaded successfully\n",p_dirent->d_name);
    } if(strncmp(p_dirent->d_name,"libxmount_morphing_",19)==0) {
      // Found possible morphing lib. Try to load it
      LIBXMOUNT_LOAD(p_library_path);

      // Load library symbols
      LIBXMOUNT_LOAD_SYMBOL("LibXmount_Morphing_GetApiVersion",
                            pfun_morphing_GetApiVersion);

      // Check library's API version
      if(pfun_morphing_GetApiVersion()!=LIBXMOUNT_MORPHING_API_VERSION) {
        LOG_DEBUG("Failed! Wrong API version.\n");
        LOG_ERROR("Unable to load morphing library '%s'. Wrong API version\n",
                  p_library_path);
        dlclose(p_libxmount);
        continue;
      }

      LIBXMOUNT_LOAD_SYMBOL("LibXmount_Morphing_GetSupportedTypes",
                            pfun_morphing_GetSupportedTypes);
      LIBXMOUNT_LOAD_SYMBOL("LibXmount_Morphing_GetFunctions",
                            pfun_morphing_GetFunctions);

      // Construct new entry for our library list
      XMOUNT_MALLOC(p_morphing_lib,pts_MorphingLib,sizeof(ts_MorphingLib));
      // Initialize lib_functions structure to NULL
      memset(&(p_morphing_lib->lib_functions),
             0,
             sizeof(ts_LibXmountMorphingFunctions));

      // Set name and handle
      XMOUNT_STRSET(p_morphing_lib->p_name,p_dirent->d_name);
      p_morphing_lib->p_lib=p_libxmount;

      // Get and set supported types
      p_supported_formats=pfun_morphing_GetSupportedTypes();
      supported_formats_len=0;
      p_buf=p_supported_formats;
      while(*p_buf!='\0') {
        supported_formats_len+=(strlen(p_buf)+1);
        p_buf+=(strlen(p_buf)+1);
      }
      supported_formats_len++;
      XMOUNT_MALLOC(p_morphing_lib->p_supported_morphing_types,
                    char*,
                    supported_formats_len);
      memcpy(p_morphing_lib->p_supported_morphing_types,
             p_supported_formats,
             supported_formats_len);

      // Get, set and check lib_functions
      pfun_morphing_GetFunctions(&(p_morphing_lib->lib_functions));
      if(p_morphing_lib->lib_functions.CreateHandle==NULL ||
         p_morphing_lib->lib_functions.DestroyHandle==NULL ||
         p_morphing_lib->lib_functions.Morph==NULL ||
         p_morphing_lib->lib_functions.Size==NULL ||
         p_morphing_lib->lib_functions.Read==NULL ||
         p_morphing_lib->lib_functions.OptionsHelp==NULL ||
         p_morphing_lib->lib_functions.OptionsParse==NULL ||
         p_morphing_lib->lib_functions.GetInfofileContent==NULL ||
         p_morphing_lib->lib_functions.GetErrorMessage==NULL ||
         p_morphing_lib->lib_functions.FreeBuffer==NULL)
      {
        LOG_DEBUG("Missing implemention of one or more functions in lib %s!\n",
                  p_dirent->d_name);
        free(p_morphing_lib->p_supported_morphing_types);
        free(p_morphing_lib->p_name);
        free(p_morphing_lib);
        dlclose(p_libxmount);
        continue;
      }

      // Add entry to the input library list
      XMOUNT_REALLOC(glob_xmount.morphing.pp_libs,
                     pts_MorphingLib*,
                     sizeof(pts_MorphingLib)*
                       (glob_xmount.morphing.libs_count+1));
      glob_xmount.morphing.pp_libs[glob_xmount.morphing.libs_count++]=
        p_morphing_lib;

      LOG_DEBUG("Morphing library '%s' loaded successfully\n",p_dirent->d_name);
    } if(strncmp(p_dirent->d_name,"libxmount_output_",17)==0) {
      // Found possible output lib. Try to load it
      LIBXMOUNT_LOAD(p_library_path);

      // Load library symbols
      LIBXMOUNT_LOAD_SYMBOL("LibXmount_Output_GetApiVersion",
                            pfun_output_GetApiVersion);

      // Check library's API version
      if(pfun_output_GetApiVersion()!=LIBXMOUNT_OUTPUT_API_VERSION) {
        LOG_DEBUG("Failed! Wrong API version.\n");
        LOG_ERROR("Unable to load output library '%s'. Wrong API version\n",
                  p_library_path);
        dlclose(p_libxmount);
        continue;
      }

      LIBXMOUNT_LOAD_SYMBOL("LibXmount_Output_GetSupportedFormats",
                            pfun_output_GetSupportedFormats);
      LIBXMOUNT_LOAD_SYMBOL("LibXmount_Output_GetFunctions",
                            pfun_output_GetFunctions);

      // Construct new entry for our library list
      XMOUNT_MALLOC(p_output_lib,pts_OutputLib,sizeof(ts_OutputLib));
      // Initialize lib_functions structure to NULL
      memset(&(p_output_lib->lib_functions),
             0,
             sizeof(ts_LibXmountOutput_Functions));

      // Set name and handle
      XMOUNT_STRSET(p_output_lib->p_name,p_dirent->d_name);
      p_output_lib->p_lib=p_libxmount;

      // Get and set supported types
      p_supported_formats=pfun_output_GetSupportedFormats();
      supported_formats_len=0;
      p_buf=p_supported_formats;
      while(*p_buf!='\0') {
        supported_formats_len+=(strlen(p_buf)+1);
        p_buf+=(strlen(p_buf)+1);
      }
      supported_formats_len++;
      XMOUNT_MALLOC(p_output_lib->p_supported_output_formats,
                    char*,
                    supported_formats_len);
      memcpy(p_output_lib->p_supported_output_formats,
             p_supported_formats,
             supported_formats_len);

      // Get, set and check lib_functions
      pfun_output_GetFunctions(&(p_output_lib->lib_functions));
      if(p_output_lib->lib_functions.CreateHandle==NULL ||
         p_output_lib->lib_functions.DestroyHandle==NULL ||
         p_output_lib->lib_functions.Transform==NULL ||
         p_output_lib->lib_functions.Size==NULL ||
         p_output_lib->lib_functions.Read==NULL ||
         p_output_lib->lib_functions.Write==NULL ||
         p_output_lib->lib_functions.OptionsHelp==NULL ||
         p_output_lib->lib_functions.OptionsParse==NULL ||
         p_output_lib->lib_functions.GetInfofileContent==NULL ||
         p_output_lib->lib_functions.GetErrorMessage==NULL ||
         p_output_lib->lib_functions.FreeBuffer==NULL)
      {
        LOG_DEBUG("Missing implemention of one or more functions in lib %s!\n",
                  p_dirent->d_name);
        free(p_output_lib->p_supported_output_formats);
        free(p_output_lib->p_name);
        free(p_output_lib);
        dlclose(p_libxmount);
        continue;
      }

      // Add entry to the input library list
      XMOUNT_REALLOC(glob_xmount.output.pp_libs,
                     pts_OutputLib*,
                     sizeof(pts_OutputLib)*
                       (glob_xmount.output.libs_count+1));
      glob_xmount.output.pp_libs[glob_xmount.output.libs_count++]=
        p_output_lib;

      LOG_DEBUG("Output library '%s' loaded successfully\n",p_dirent->d_name);
    } else {
      LOG_DEBUG("Ignoring '%s'.\n",p_dirent->d_name);
      continue;
    }
  }

#undef LIBXMOUNT_LOAD_SYMBOL
#undef LIBXMOUNT_LOAD

  LOG_DEBUG("A total of %u input libs, %u morphing libs and %u output libs "
              "were loaded.\n",
            glob_xmount.input.libs_count,
            glob_xmount.morphing.libs_count,
            glob_xmount.output.libs_count);

  free(p_library_path);
  closedir(p_dir);
  return ((glob_xmount.input.libs_count>0 &&
           glob_xmount.morphing.libs_count>0 &&
           glob_xmount.output.libs_count>0) ? TRUE : FALSE);
}

//! Search an appropriate input lib for specified input type
/*!
 * \param p_input_image Input image to search input lib for
 * \return TRUE on success, FALSE on error
 */
static int FindInputLib(pts_InputImage p_input_image) {
  char *p_buf;

  LOG_DEBUG("Trying to find suitable library for input type '%s'.\n",
            p_input_image->p_type);

  // Loop over all loaded libs
  for(uint32_t i=0;i<glob_xmount.input.libs_count;i++) {
    LOG_DEBUG("Checking input library %s\n",
              glob_xmount.input.pp_libs[i]->p_name);
    p_buf=glob_xmount.input.pp_libs[i]->p_supported_input_types;
    while(*p_buf!='\0') {
      if(strcmp(p_buf,p_input_image->p_type)==0) {
        // Library supports input type, set lib functions
        LOG_DEBUG("Input library '%s' pretends to handle that input type.\n",
                  glob_xmount.input.pp_libs[i]->p_name);
        p_input_image->p_functions=
          &(glob_xmount.input.pp_libs[i]->lib_functions);
        return TRUE;
      }
      p_buf+=(strlen(p_buf)+1);
    }
  }

  LOG_DEBUG("Couldn't find any suitable library.\n");

  // No library supporting input type found
  return FALSE;
}

//! Search an appropriate morphing lib for the specified morph type
/*!
 * \return TRUE on success, FALSE on error
 */
static int FindMorphingLib() {
  char *p_buf;

  LOG_DEBUG("Trying to find suitable library for morph type '%s'.\n",
            glob_xmount.morphing.p_morph_type);

  // Loop over all loaded libs
  for(uint32_t i=0;i<glob_xmount.morphing.libs_count;i++) {
    LOG_DEBUG("Checking morphing library %s\n",
              glob_xmount.morphing.pp_libs[i]->p_name);
    p_buf=glob_xmount.morphing.pp_libs[i]->p_supported_morphing_types;
    while(*p_buf!='\0') {
      if(strcmp(p_buf,glob_xmount.morphing.p_morph_type)==0) {
        // Library supports morph type, set lib functions
        LOG_DEBUG("Morphing library '%s' pretends to handle that morph type.\n",
                  glob_xmount.morphing.pp_libs[i]->p_name);
        glob_xmount.morphing.p_functions=
          &(glob_xmount.morphing.pp_libs[i]->lib_functions);
        return TRUE;
      }
      p_buf+=(strlen(p_buf)+1);
    }
  }

  LOG_DEBUG("Couldn't find any suitable library.\n");

  // No library supporting morph type found
  return FALSE;
}

//! Search an appropriate output lib for the specified output format
/*!
 * \return TRUE on success, FALSE on error
 */
static int FindOutputLib() {
  char *p_buf;

  LOG_DEBUG("Trying to find suitable library for output format '%s'.\n",
            glob_xmount.output.p_output_format);

  // Loop over all loaded output libs
  for(uint32_t i=0;i<glob_xmount.output.libs_count;i++) {
    LOG_DEBUG("Checking output library %s\n",
              glob_xmount.output.pp_libs[i]->p_name);
    p_buf=glob_xmount.output.pp_libs[i]->p_supported_output_formats;
    while(*p_buf!='\0') {
      if(strcmp(p_buf,glob_xmount.output.p_output_format)==0) {
        // Library supports output type, set lib functions
        LOG_DEBUG("Output library '%s' pretends to handle that output format.\n",
                  glob_xmount.output.pp_libs[i]->p_name);
        glob_xmount.output.p_functions=
          &(glob_xmount.output.pp_libs[i]->lib_functions);
        return TRUE;
      }
      p_buf+=(strlen(p_buf)+1);
    }
  }

  LOG_DEBUG("Couldn't find any suitable library.\n");

  // No library supporting output format found
  return FALSE;
}

static void InitResources() {
  // Args
  glob_xmount.args.overwrite_cache=FALSE;
  glob_xmount.args.p_cache_file=NULL;

  // Input
  glob_xmount.input.libs_count=0;
  glob_xmount.input.pp_libs=NULL;
  glob_xmount.input.lib_params_count=0;
  glob_xmount.input.pp_lib_params=NULL;
  glob_xmount.input.images_count=0;
  glob_xmount.input.pp_images=NULL;
  glob_xmount.input.image_offset=0;
  glob_xmount.input.image_size_limit=0;
  glob_xmount.input.image_hash_lo=0;
  glob_xmount.input.image_hash_hi=0;

  // Morphing
  glob_xmount.morphing.libs_count=0;
  glob_xmount.morphing.pp_libs=NULL;
  glob_xmount.morphing.p_morph_type=NULL;
  glob_xmount.morphing.lib_params_count=0;
  glob_xmount.morphing.pp_lib_params=NULL;
  glob_xmount.morphing.p_handle=NULL;
  glob_xmount.morphing.p_functions=NULL;
  glob_xmount.morphing.input_image_functions.ImageCount=
    &LibXmount_Morphing_ImageCount;
  glob_xmount.morphing.input_image_functions.Size=&LibXmount_Morphing_Size;
  glob_xmount.morphing.input_image_functions.Read=&LibXmount_Morphing_Read;

  // Cache
  glob_xmount.h_cache=NULL;

  // Output
  glob_xmount.output.libs_count=0;
  glob_xmount.output.pp_libs=NULL;
  glob_xmount.output.p_output_format=NULL;
  glob_xmount.output.lib_params_count=0;
  glob_xmount.output.pp_lib_params=NULL;
  glob_xmount.output.p_handle=NULL;
  glob_xmount.output.p_functions=NULL;
  glob_xmount.output.input_functions.Size=&LibXmount_Output_Size;
  glob_xmount.output.input_functions.Read=&LibXmount_Output_Read;
  glob_xmount.output.input_functions.Write=&LibXmount_Output_Write;
  glob_xmount.output.image_size=0;
  glob_xmount.output.writable=FALSE;
  glob_xmount.output.p_virtual_image_path=NULL;
  glob_xmount.output.p_info_path=NULL;
  glob_xmount.output.p_info_file=NULL;

  // Misc data
  glob_xmount.debug=FALSE;
  glob_xmount.may_set_fuse_allow_other=FALSE;
  glob_xmount.fuse_argc=0;
  glob_xmount.pp_fuse_argv=NULL;
  glob_xmount.p_mountpoint=NULL;
}

/*
 * FreeResources
 */
static void FreeResources() {
  int ret;
  te_XmountCache_Error cache_ret=e_XmountCache_Error_None;

  LOG_DEBUG("Freeing all resources\n");

  // Misc
  if(glob_xmount.pp_fuse_argv!=NULL) {
    for(int i=0;i<glob_xmount.fuse_argc;i++) free(glob_xmount.pp_fuse_argv[i]);
    free(glob_xmount.pp_fuse_argv);
  }
  if(glob_xmount.p_mountpoint!=NULL) free(glob_xmount.p_mountpoint);

  // Output
  if(glob_xmount.output.p_functions!=NULL) {
    if(glob_xmount.output.p_handle!=NULL) {
      // Destroy output handle
      ret=glob_xmount.output.p_functions->
            DestroyHandle(&(glob_xmount.output.p_handle));
      if(ret!=0) {
        LOG_ERROR("Unable to destroy output handle: %s!\n",
                  glob_xmount.output.p_functions->GetErrorMessage(ret));
      }
    }
  }
  if(glob_xmount.output.pp_lib_params!=NULL) {
    for(uint32_t i=0;i<glob_xmount.output.lib_params_count;i++)
      free(glob_xmount.output.pp_lib_params[i]);
    free(glob_xmount.output.pp_lib_params);
  }
  if(glob_xmount.output.p_output_format!=NULL)
    free(glob_xmount.output.p_output_format);
  if(glob_xmount.output.pp_libs!=NULL) {
    // Unload output libs
    for(uint32_t i=0;i<glob_xmount.output.libs_count;i++) {
      if(glob_xmount.output.pp_libs[i]==NULL) continue;
      if(glob_xmount.output.pp_libs[i]->p_supported_output_formats!=NULL)
        free(glob_xmount.output.pp_libs[i]->p_supported_output_formats);
      if(glob_xmount.output.pp_libs[i]->p_lib!=NULL)
        dlclose(glob_xmount.output.pp_libs[i]->p_lib);
      if(glob_xmount.output.pp_libs[i]->p_name!=NULL)
        free(glob_xmount.output.pp_libs[i]->p_name);
      free(glob_xmount.output.pp_libs[i]);
    }
    free(glob_xmount.output.pp_libs);
  }
  if(glob_xmount.output.p_info_path!=NULL)
    free(glob_xmount.output.p_info_path);
  if(glob_xmount.output.p_info_file!=NULL)
    free(glob_xmount.output.p_info_file);
  if(glob_xmount.output.p_virtual_image_path!=NULL)
    free(glob_xmount.output.p_virtual_image_path);

  // Cache
  if(glob_xmount.h_cache!=NULL) {
    cache_ret=XmountCache_Close(&(glob_xmount.h_cache));
    if(cache_ret!=e_XmountCache_Error_None) {
      LOG_ERROR("Unable to close cache file: Error code %u: Ignoring!\n",
                cache_ret);
    }
  }

  // Morphing
  if(glob_xmount.morphing.p_functions!=NULL) {
    if(glob_xmount.morphing.p_handle!=NULL) {
      // Destroy morphing handle
      ret=glob_xmount.morphing.p_functions->
            DestroyHandle(&(glob_xmount.morphing.p_handle));
      if(ret!=0) {
        LOG_ERROR("Unable to destroy morphing handle: %s!\n",
                  glob_xmount.morphing.p_functions->GetErrorMessage(ret));
      }
    }
  }
  if(glob_xmount.morphing.pp_lib_params!=NULL) {
    for(uint32_t i=0;i<glob_xmount.morphing.lib_params_count;i++)
      free(glob_xmount.morphing.pp_lib_params[i]);
    free(glob_xmount.morphing.pp_lib_params);
  }
  if(glob_xmount.morphing.p_morph_type!=NULL)
    free(glob_xmount.morphing.p_morph_type);
  if(glob_xmount.morphing.pp_libs!=NULL) {
    // Unload morphing libs
    for(uint32_t i=0;i<glob_xmount.morphing.libs_count;i++) {
      if(glob_xmount.morphing.pp_libs[i]==NULL) continue;
      if(glob_xmount.morphing.pp_libs[i]->p_supported_morphing_types!=NULL)
        free(glob_xmount.morphing.pp_libs[i]->p_supported_morphing_types);
      if(glob_xmount.morphing.pp_libs[i]->p_lib!=NULL)
        dlclose(glob_xmount.morphing.pp_libs[i]->p_lib);
      if(glob_xmount.morphing.pp_libs[i]->p_name!=NULL)
        free(glob_xmount.morphing.pp_libs[i]->p_name);
      free(glob_xmount.morphing.pp_libs[i]);
    }
    free(glob_xmount.morphing.pp_libs);
  }

  // Input
  if(glob_xmount.input.pp_images!=NULL) {
    // Close all input images
    for(uint64_t i=0;i<glob_xmount.input.images_count;i++) {
      if(glob_xmount.input.pp_images[i]==NULL) continue;
      if(glob_xmount.input.pp_images[i]->p_functions!=NULL) {
        if(glob_xmount.input.pp_images[i]->p_handle!=NULL) {
          ret=glob_xmount.input.pp_images[i]->p_functions->
                Close(glob_xmount.input.pp_images[i]->p_handle);
          if(ret!=0) {
            LOG_ERROR("Unable to close input image: %s\n",
                      glob_xmount.input.pp_images[i]->p_functions->
                        GetErrorMessage(ret));
          }
          ret=glob_xmount.input.pp_images[i]->p_functions->
                DestroyHandle(&(glob_xmount.input.pp_images[i]->p_handle));
          if(ret!=0) {
            LOG_ERROR("Unable to destroy input image handle: %s\n",
                      glob_xmount.input.pp_images[i]->p_functions->
                        GetErrorMessage(ret));
          }
        }
      }
      if(glob_xmount.input.pp_images[i]->pp_files!=NULL) {
        for(uint64_t ii=0;ii<glob_xmount.input.pp_images[i]->files_count;ii++) {
          if(glob_xmount.input.pp_images[i]->pp_files[ii]!=NULL)
            free(glob_xmount.input.pp_images[i]->pp_files[ii]);
        }
        free(glob_xmount.input.pp_images[i]->pp_files);
      }
      if(glob_xmount.input.pp_images[i]->p_type!=NULL)
        free(glob_xmount.input.pp_images[i]->p_type);
      free(glob_xmount.input.pp_images[i]);
    }
    free(glob_xmount.input.pp_images);
  }
  if(glob_xmount.input.pp_lib_params!=NULL) {
    for(uint32_t i=0;i<glob_xmount.input.lib_params_count;i++)
      free(glob_xmount.input.pp_lib_params[i]);
    free(glob_xmount.input.pp_lib_params);
  }
  if(glob_xmount.input.pp_libs!=NULL) {
    // Unload all input libs
    for(uint32_t i=0;i<glob_xmount.input.libs_count;i++) {
      if(glob_xmount.input.pp_libs[i]->p_supported_input_types!=NULL)
        free(glob_xmount.input.pp_libs[i]->p_supported_input_types);
      if(glob_xmount.input.pp_libs[i]->p_lib!=NULL)
        dlclose(glob_xmount.input.pp_libs[i]->p_lib);
      if(glob_xmount.input.pp_libs[i]->p_name!=NULL)
        free(glob_xmount.input.pp_libs[i]->p_name);
      free(glob_xmount.input.pp_libs[i]);
    }
    free(glob_xmount.input.pp_libs);
  }

  // Args
  if(glob_xmount.args.p_cache_file!=NULL) {
    XMOUNT_FREE(glob_xmount.args.p_cache_file);
  }

  // Before we return, initialize everything in case ReleaseResources would be
  // called again.
  InitResources();
}

//! Function to split given library options
static int SplitLibraryParameters(char *p_params,
                                  uint32_t *p_ret_opts_count,
                                  pts_LibXmountOptions **ppp_ret_opt)
{
  pts_LibXmountOptions p_opts=NULL;
  pts_LibXmountOptions *pp_opts=NULL;
  uint32_t params_len;
  uint32_t opts_count=0;
  uint32_t sep_pos=0;
  char *p_buf=p_params;

  if(p_params==NULL) return FALSE;

  // Get params length
  params_len=strlen(p_params);

  // Return if no params specified
  if(params_len==0) {
    *ppp_ret_opt=NULL;
    p_ret_opts_count=0;
    return TRUE;
  }

  // Split params
  while(*p_buf!='\0') {
    XMOUNT_MALLOC(p_opts,pts_LibXmountOptions,sizeof(ts_LibXmountOptions));
    p_opts->valid=0;

#define FREE_PP_OPTS() {                                 \
  if(pp_opts!=NULL) {                                    \
    for(uint32_t i=0;i<opts_count;i++) free(pp_opts[i]); \
    free(pp_opts);                                       \
  }                                                      \
}

    // Search next assignment operator
    sep_pos=0;
    while(p_buf[sep_pos]!='\0' &&  p_buf[sep_pos]!='=') sep_pos++;
    if(sep_pos==0 || p_buf[sep_pos]=='\0') {
      LOG_ERROR("Library parameter '%s' is missing an assignment operator!\n",
                p_buf);
      free(p_opts);
      FREE_PP_OPTS();
      return FALSE;
    }

    // Save option key
    XMOUNT_STRNSET(p_opts->p_key,p_buf,sep_pos);
    p_buf+=(sep_pos+1);

    // Search next separator
    sep_pos=0;
    while(p_buf[sep_pos]!='\0' &&  p_buf[sep_pos]!=',') sep_pos++;
    if(sep_pos==0) {
      LOG_ERROR("Library parameter '%s' is not of format key=value!\n",
                p_opts->p_key);
      free(p_opts->p_key);
      free(p_opts);
      FREE_PP_OPTS();
      return FALSE;
    }

    // Save option value
    XMOUNT_STRNSET(p_opts->p_value,p_buf,sep_pos);
    p_buf+=sep_pos;

    LOG_DEBUG("Extracted library option: '%s' = '%s'\n",
              p_opts->p_key,
              p_opts->p_value);

#undef FREE_PP_OPTS

    // Add current option to return array
    XMOUNT_REALLOC(pp_opts,
                   pts_LibXmountOptions*,
                   sizeof(pts_LibXmountOptions)*(opts_count+1));
    pp_opts[opts_count++]=p_opts;

    // If we're not at the end of p_params, skip over separator for next run
    if(*p_buf!='\0') p_buf++;
  }

  LOG_DEBUG("Extracted a total of %" PRIu32 " library options\n",opts_count);

  *p_ret_opts_count=opts_count;
  *ppp_ret_opt=pp_opts;
  return TRUE;
}

/*******************************************************************************
 * LibXmount_Morphing function implementation
 ******************************************************************************/
//! Function to get the amount of input images
/*!
 * \param p_count Count of input images
 * \return 0 on success
 */
static int LibXmount_Morphing_ImageCount(uint64_t *p_count) {
  *p_count=glob_xmount.input.images_count;
  return 0;
}

//! Function to get the size of the morphed data
/*!
 * \param image Image number
 * \param p_size Pointer to store input image's size to
 * \return 0 on success
 */
static int LibXmount_Morphing_Size(uint64_t image, uint64_t *p_size) {
  if(image>=glob_xmount.input.images_count) return -1;
  *p_size=glob_xmount.input.pp_images[image]->size;
  return 0;
}

//! Function to read data from input image
/*!
 * \param image Image number
 * \param p_buf Buffer to store read data to
 * \param offset Position at which to start reading
 * \param count Amount of bytes to read
 * \param p_read Number of read bytes on success
 * \return 0 on success or negated error code on error
 */
static int LibXmount_Morphing_Read(uint64_t image,
                                   char *p_buf,
                                   off_t offset,
                                   size_t count,
                                   size_t *p_read)
{
  if(image>=glob_xmount.input.images_count) return -EIO;
  return ReadInputImageData(glob_xmount.input.pp_images[image],
                            p_buf,
                            offset,
                            count,
                            p_read);
}

/*******************************************************************************
 * LibXmount_Output function implementation
 ******************************************************************************/
//! Function to get the size of the morphed image
/*!
 * \param p_size Pointer to store morphed image's size to
 * \return 0 on success
 */
static int LibXmount_Output_Size(uint64_t *p_size) {
  int ret=0;

  ret=GetMorphedImageSize(p_size);

  return ret==TRUE ? 0 : ret;
}

//! Function to read data from the morphed image
/*!
 * \param p_buf Buffer to store read data to
 * \param offset Position at which to start reading
 * \param count Amount of bytes to read
 * \param p_read Number of read bytes on success
 * \return 0 on success or negated error code on error
 */
static int LibXmount_Output_Read(char *p_buf,
                                 off_t offset,
                                 size_t count,
                                 size_t *p_read)
{
  return ReadMorphedImageData(p_buf,offset,count,p_read);
}

//! Function to write data to the morphed image
/*!
 * \param p_buf Buffer with data to write
 * \param offset Position at which to start writing
 * \param count Amount of bytes to write
 * \param p_written Number of written bytes on success
 * \return 0 on success or negated error code on error
 */
static int LibXmount_Output_Write(char *p_buf,
                                  off_t offset,
                                  size_t count,
                                  size_t *p_written)
{
  return WriteMorphedImageData(p_buf,offset,count,p_written);
}

/*******************************************************************************
 * Main
 ******************************************************************************/
int main(int argc, char *argv[]) {
  struct stat file_stat;
  int ret;
  int fuse_ret;
  char *p_err_msg;
  te_XmountCache_Error cache_ret=e_XmountCache_Error_None;

  // Set implemented FUSE functions
  struct fuse_operations xmount_operations = {
    //.access=FuseAccess,
    .getattr=Xmount_FuseGetAttr,
    .mkdir=Xmount_FuseMkDir,
    .mknod=Xmount_FuseMkNod,
    .open=Xmount_FuseOpen,
    .readdir=Xmount_FuseReadDir,
    .read=Xmount_FuseRead,
    .rename=Xmount_FuseRename,
    .rmdir=Xmount_FuseRmDir,
    //.statfs=FuseStatFs,
    .unlink=Xmount_FuseUnlink,
    .write=Xmount_FuseWrite
  };

  // Disable std output / input buffering
  setbuf(stdout,NULL);
  setbuf(stderr,NULL);

  // Init glob_xmount
  InitResources();

  // Load input, morphing and output libs
  if(!LoadLibs()) {
    LOG_ERROR("Unable to load any libraries!\n")
    return 1;
  }

  // Check FUSE settings
  CheckFuseSettings();

  // Parse command line options
  if(ParseCmdLine(argc,argv)!=TRUE) {
    PrintUsage(argv[0]);
    FreeResources();
    return 1;
  }

  // Check command line options
  if(glob_xmount.input.images_count==0) {
    LOG_ERROR("No --in command line option specified!\n")
    PrintUsage(argv[0]);
    FreeResources();
    return 1;
  }
  if(glob_xmount.fuse_argc<2) {
    LOG_ERROR("Couldn't parse command line options!\n")
    PrintUsage(argv[0]);
    FreeResources();
    return 1;
  }
  if(glob_xmount.morphing.p_morph_type==NULL) {
    XMOUNT_STRSET(glob_xmount.morphing.p_morph_type,"combine");
  }
  // TODO: Add default output image format here

  // Check if mountpoint is a valid dir
  if(stat(glob_xmount.p_mountpoint,&file_stat)!=0) {
    LOG_ERROR("Unable to stat mount point '%s'!\n",glob_xmount.p_mountpoint);
    PrintUsage(argv[0]);
    FreeResources();
    return 1;
  }
  if(!S_ISDIR(file_stat.st_mode)) {
    LOG_ERROR("Mount point '%s' is not a directory!\n",
              glob_xmount.p_mountpoint);
    PrintUsage(argv[0]);
    FreeResources();
    return 1;
  }

  if(glob_xmount.debug==TRUE) {
    LOG_DEBUG("Options passed to FUSE: ")
    for(int i=0;i<glob_xmount.fuse_argc;i++) {
      printf("%s ",glob_xmount.pp_fuse_argv[i]);
    }
    printf("\n");
  }

  // Init mutexes
  pthread_mutex_init(&(glob_xmount.mutex_image_rw),NULL);
  pthread_mutex_init(&(glob_xmount.mutex_info_read),NULL);

  // Load input images
  for(uint64_t i=0;i<glob_xmount.input.images_count;i++) {
    if(glob_xmount.debug==TRUE) {
      if(glob_xmount.input.pp_images[i]->files_count==1) {
        LOG_DEBUG("Loading image file \"%s\"...\n",
                  glob_xmount.input.pp_images[i]->pp_files[0])
      } else {
        LOG_DEBUG("Loading image files \"%s .. %s\"...\n",
                  glob_xmount.input.pp_images[i]->pp_files[0],
                  glob_xmount.input.pp_images[i]->
                    pp_files[glob_xmount.input.pp_images[i]->files_count-1])
      }
    }

    // Find input lib
    if(!FindInputLib(glob_xmount.input.pp_images[i])) {
      LOG_ERROR("Unknown input image type '%s' for input image '%s'!\n",
                glob_xmount.input.pp_images[i]->p_type,
                glob_xmount.input.pp_images[i]->pp_files[0])
      PrintUsage(argv[0]);
      FreeResources();
      return 1;
    }

    // Init input image handle
    ret=glob_xmount.input.pp_images[i]->p_functions->
          CreateHandle(&(glob_xmount.input.pp_images[i]->p_handle),
                       glob_xmount.input.pp_images[i]->p_type,
                       glob_xmount.debug);
    if(ret!=0) {
      LOG_ERROR("Unable to init input handle for input image '%s': %s!\n",
                glob_xmount.input.pp_images[i]->pp_files[0],
                glob_xmount.input.pp_images[i]->p_functions->
                  GetErrorMessage(ret));
      FreeResources();
      return 1;
    }

    // Parse input lib specific options
    if(glob_xmount.input.pp_lib_params!=NULL) {
      ret=glob_xmount.input.pp_images[i]->p_functions->
            OptionsParse(glob_xmount.input.pp_images[i]->p_handle,
                         glob_xmount.input.lib_params_count,
                         glob_xmount.input.pp_lib_params,
                         (const char**)&p_err_msg);
      if(ret!=0) {
        if(p_err_msg!=NULL) {
          LOG_ERROR("Unable to parse input library specific options for image "
                      "'%s': %s: %s!\n",
                    glob_xmount.input.pp_images[i]->pp_files[0],
                    glob_xmount.input.pp_images[i]->p_functions->
                      GetErrorMessage(ret),
                    p_err_msg);
          glob_xmount.input.pp_images[i]->p_functions->FreeBuffer(p_err_msg);
          FreeResources();
          return 1;
        } else {
          LOG_ERROR("Unable to parse input library specific options for image "
                      "'%s': %s!\n",
                    glob_xmount.input.pp_images[i]->pp_files[0],
                    glob_xmount.input.pp_images[i]->p_functions->
                      GetErrorMessage(ret));
          FreeResources();
          return 1;
        }
      }
    }

    // Open input image
    ret=
      glob_xmount.input.pp_images[i]->
        p_functions->
          Open(glob_xmount.input.pp_images[i]->p_handle,
               (const char**)(glob_xmount.input.pp_images[i]->pp_files),
               glob_xmount.input.pp_images[i]->files_count);
    if(ret!=0) {
      LOG_ERROR("Unable to open input image file '%s': %s!\n",
                glob_xmount.input.pp_images[i]->pp_files[0],
                glob_xmount.input.pp_images[i]->p_functions->
                  GetErrorMessage(ret));
      FreeResources();
      return 1;
    }

    // Determine input image size
    ret=glob_xmount.input.pp_images[i]->
      p_functions->
        Size(glob_xmount.input.pp_images[i]->p_handle,
             &(glob_xmount.input.pp_images[i]->size));
    if(ret!=0) {
      LOG_ERROR("Unable to determine size of input image '%s': %s!\n",
                glob_xmount.input.pp_images[i]->pp_files[0],
                glob_xmount.input.pp_images[i]->
                  p_functions->GetErrorMessage(ret));
      FreeResources();
      return 1;
    }

    // If an offset was specified, check it against offset and change size
    if(glob_xmount.input.image_offset!=0) {
      if(glob_xmount.input.image_offset>glob_xmount.input.pp_images[i]->size) {
        LOG_ERROR("The specified offset is larger than the size of the input "
                    "image '%s'! (%" PRIu64 " > %" PRIu64 ")\n",
                  glob_xmount.input.pp_images[i]->pp_files[0],
                  glob_xmount.input.image_offset,
                  glob_xmount.input.pp_images[i]->size);
        FreeResources();
        return 1;
      }
      glob_xmount.input.pp_images[i]->size-=glob_xmount.input.image_offset;
    }

    // If a size limit was specified, check it and change size
    if(glob_xmount.input.image_size_limit!=0) {
      if(glob_xmount.input.pp_images[i]->size<
           glob_xmount.input.image_size_limit)
      {
        LOG_ERROR("The specified size limit is larger than the size of the "
                    "input image '%s'! (%" PRIu64 " > %" PRIu64 ")\n",
                  glob_xmount.input.pp_images[i]->pp_files[0],
                  glob_xmount.input.image_size_limit,
                  glob_xmount.input.pp_images[i]->size);
        FreeResources();
        return 1;
      }
      glob_xmount.input.pp_images[i]->size=glob_xmount.input.image_size_limit;
    }

    LOG_DEBUG("Input image loaded successfully\n")
  }

  // Find morphing lib
  if(FindMorphingLib()!=TRUE) {
    LOG_ERROR("Unable to find a library supporting the morphing type '%s'!\n",
              glob_xmount.morphing.p_morph_type);
    FreeResources();
    return 1;
  }

  // Init morphing
  ret=glob_xmount.morphing.p_functions->
        CreateHandle(&glob_xmount.morphing.p_handle,
                     glob_xmount.morphing.p_morph_type,
                     glob_xmount.debug);
  if(ret!=0) {
    LOG_ERROR("Unable to create morphing handle: %s!\n",
              glob_xmount.morphing.p_functions->GetErrorMessage(ret));
    FreeResources();
    return 1;
  }

  // Parse morphing lib specific options
  if(glob_xmount.morphing.pp_lib_params!=NULL) {
    p_err_msg=NULL;
    ret=glob_xmount.morphing.p_functions->
          OptionsParse(glob_xmount.morphing.p_handle,
                       glob_xmount.morphing.lib_params_count,
                       glob_xmount.morphing.pp_lib_params,
                       (const char**)&p_err_msg);
    if(ret!=0) {
      if(p_err_msg!=NULL) {
        LOG_ERROR("Unable to parse morphing library specific options: %s: %s!\n",
                  glob_xmount.morphing.p_functions->GetErrorMessage(ret),
                  p_err_msg);
        glob_xmount.morphing.p_functions->FreeBuffer(p_err_msg);
        FreeResources();
        return 1;
      } else {
        LOG_ERROR("Unable to parse morphing library specific options: %s!\n",
                  glob_xmount.morphing.p_functions->GetErrorMessage(ret));
        FreeResources();
        return 1;
      }
    }
  }

  // Morph image
  ret=glob_xmount.morphing.p_functions->
        Morph(glob_xmount.morphing.p_handle,
              &(glob_xmount.morphing.input_image_functions));
  if(ret!=0) {
    LOG_ERROR("Unable to start morphing: %s!\n",
              glob_xmount.morphing.p_functions->GetErrorMessage(ret));
    FreeResources();
    return 1;
  }

  // Init random generator
  srand(time(NULL));

  // Calculate partial MD5 hash of input image file
  if(CalculateInputImageHash(&(glob_xmount.input.image_hash_lo),
                             &(glob_xmount.input.image_hash_hi))==FALSE)
  {
    LOG_ERROR("Couldn't calculate partial hash of morphed image!\n")
    return 1;
  }

  if(glob_xmount.debug==TRUE) {
    LOG_DEBUG("Partial MD5 hash of morphed image: ")
    for(int i=0;i<8;i++)
      printf("%02hhx",*(((char*)(&(glob_xmount.input.image_hash_lo)))+i));
    for(int i=0;i<8;i++)
      printf("%02hhx",*(((char*)(&(glob_xmount.input.image_hash_hi)))+i));
    printf("\n");
  }

  if(!ExtractOutputFileNames(glob_xmount.input.pp_images[0]->pp_files[0])) {
    LOG_ERROR("Couldn't extract virtual file names!\n");
    FreeResources();
    return 1;
  }
  LOG_DEBUG("Virtual file names extracted successfully\n")

  // Find output lib
  if(FindOutputLib()!=TRUE) {
    LOG_ERROR("Unable to find a library supporting the output format '%s'!\n",
              glob_xmount.output.p_output_format);
    FreeResources();
    return 1;
  }

  // Init output
  ret=glob_xmount.output.p_functions->
        CreateHandle(&glob_xmount.output.p_handle,
                     glob_xmount.output.p_output_format,
                     glob_xmount.debug);
  if(ret!=0) {
    LOG_ERROR("Unable to create output handle: %s!\n",
              glob_xmount.output.p_functions->GetErrorMessage(ret));
    FreeResources();
    return 1;
  }

  // Parse output lib specific options
  if(glob_xmount.output.pp_lib_params!=NULL) {
    p_err_msg=NULL;
    ret=glob_xmount.output.p_functions->
          OptionsParse(glob_xmount.output.p_handle,
                       glob_xmount.output.lib_params_count,
                       glob_xmount.output.pp_lib_params,
                       (const char**)&p_err_msg);
    if(ret!=0) {
      if(p_err_msg!=NULL) {
        LOG_ERROR("Unable to parse output library specific options: %s: %s!\n",
                  glob_xmount.output.p_functions->GetErrorMessage(ret),
                  p_err_msg);
        glob_xmount.output.p_functions->FreeBuffer(p_err_msg);
        FreeResources();
        return 1;
      } else {
        LOG_ERROR("Unable to parse output library specific options: %s!\n",
                  glob_xmount.output.p_functions->GetErrorMessage(ret));
        FreeResources();
        return 1;
      }
    }
  }

  // Morph image
  ret=glob_xmount.morphing.p_functions->
        Morph(glob_xmount.morphing.p_handle,
              &(glob_xmount.morphing.input_image_functions));
  if(ret!=0) {
    LOG_ERROR("Unable to start morphing: %s!\n",
              glob_xmount.morphing.p_functions->GetErrorMessage(ret));
    FreeResources();
    return 1;
  }

  // Gather infos for info file
  if(!InitInfoFile()) {
    LOG_ERROR("Couldn't gather infos for virtual image info file!\n")
    FreeResources();
    return 1;
  }
  LOG_DEBUG("Virtual image info file build successfully\n")

  if(glob_xmount.output.writable) {
    // Init cache file and cache file block index
    // TODO: Add cache file creration / opening
/*
    if(glob_xmount.args.overwrite_cache==TRUE) {
      cache_ret=XmountCache_Create(&(glob_xmount.h_cache),
                                   glob_xmount.args.p_cache_file,


    } else {
      cache_ret=XmountCache_Open
    }


    te_XmountCache_Error cache_ret=e_XmountCache_Error_None;
    if(!InitCacheFile()) {
      LOG_ERROR("Couldn't initialize cache file!\n")
      FreeResources();
      return 1;
    }
*/
    LOG_DEBUG("Cache file initialized successfully\n")
  }

  // Call fuse_main to do the fuse magic
  fuse_ret=fuse_main(glob_xmount.fuse_argc,
                     glob_xmount.pp_fuse_argv,
                     &xmount_operations,
                     NULL);

  // Destroy mutexes
  pthread_mutex_destroy(&(glob_xmount.mutex_image_rw));
  pthread_mutex_destroy(&(glob_xmount.mutex_info_read));

  // Free allocated memory
  FreeResources();
  return fuse_ret;
}
