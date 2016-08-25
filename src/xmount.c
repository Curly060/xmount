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

#define XMOUNT_COPYRIGHT_NOTICE \
  "xmount v%s Copyright (c) 2008-2016 by Gillen Daniel <gillen.dan@pinguin.lu>"

#define IMAGE_INFO_INPUT_HEADER \
  "------> The following values are supplied by the used input library(ies) " \
    "<------\n"

#define IMAGE_INFO_MORPHING_HEADER \
  "\n------> The following values are supplied by the used morphing library " \
    "<------\n\n"

#define LOG_WARNING(...) do {         \
  LIBXMOUNT_LOG_WARNING(__VA_ARGS__); \
} while(0)

#define LOG_ERROR(...) do {         \
  LIBXMOUNT_LOG_ERROR(__VA_ARGS__); \
} while(0)

#define LOG_DEBUG(...) do {                           \
  LIBXMOUNT_LOG_DEBUG(glob_xmount.debug,__VA_ARGS__); \
} while(0)

#define HASH_AMOUNT (1024*1024)*10 // Amount of data used to construct a
                                   // "unique" hash for every input image
                                   // (10MByte)

/*******************************************************************************
 * Forward declarations
 ******************************************************************************/
/*
 * Misc
 */
static int InitResources();
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
/*
 * Functions exported to LibXmount_Morphing
 */
static int LibXmount_Morphing_ImageCount(uint64_t*);
static int LibXmount_Morphing_Size(uint64_t, uint64_t*);
static int LibXmount_Morphing_Read(uint64_t, char*, off_t, size_t, size_t*);
static int LibXmount_Morphing_Write(uint64_t, char*, off_t, size_t, size_t*);
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

#define PRINTUSAGE__LIST_SUPP_LIB_TYPES(fun,handle,ret_ok) do { \
  first=1;                                                      \
  if(fun(handle,&p_buf)==ret_ok) {                              \
    while(*p_buf!='\0') {                                       \
      if(first==1) {                                            \
        printf("\"%s\"",p_buf);                                 \
        first=0;                                                \
      } else printf(", \"%s\"",p_buf);                          \
      p_buf+=(strlen(p_buf)+1);                                 \
    }                                                           \
    free(p_buf);                                                \
  }                                                             \
  printf(".\n");                                                \
} while(0)

  // List supported input formats
  PRINTUSAGE__LIST_SUPP_LIB_TYPES(XmountInput_GetSupportedFormats,
                                  glob_xmount.h_input,
                                  e_XmountInput_Error_None);

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
  PRINTUSAGE__LIST_SUPP_LIB_TYPES(XmountMorphing_GetSupportedTypes,
                                  glob_xmount.h_morphing,
                                  e_XmountMorphError_None);

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

  // List supported morphing functions
  PRINTUSAGE__LIST_SUPP_LIB_TYPES(XmountOutput_GetSupportedFormats,
                                  glob_xmount.h_output,
                                  e_XmountOutputError_None);

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
  printf("Input / Morphing / Output library specific options:\n");
  printf("  Input / Morphing libraries might support an own set of "
           "options to configure / tune their behaviour.\n");
  printf("  Libraries supporting this feature (if any) and their "
           "options are listed below.\n");
  printf("\n");

#define PRINTUSAGE__LIST_LIB_OPT_HELP(fun,handle,ret_ok) do { \
  if(fun(handle,&p_buf)==ret_ok) {                            \
    printf("%s",p_buf);                                       \
    free(p_buf);                                              \
  }                                                           \
} while(0)

  // List input, morphing and output lib options
  PRINTUSAGE__LIST_LIB_OPT_HELP(XmountInput_GetOptionsHelpText,
                                glob_xmount.h_input,
                                e_XmountInput_Error_None);
  PRINTUSAGE__LIST_LIB_OPT_HELP(XmountMorphing_GetOptionsHelpText,
                                glob_xmount.h_morphing,
                                e_XmountMorphError_None);
  PRINTUSAGE__LIST_LIB_OPT_HELP(XmountOutput_GetOptionsHelpText,
                                glob_xmount.h_output,
                                e_XmountOutputError_None);

#undef PRINTUSAGE__LIST_LIB_OPT_HELP
#undef PRINTUSAGE__LIST_SUPPORTED_LIB_OPTS

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
  uint64_t buf;
  char *p_buf;
  char **pp_buf;
  int ret;
  te_XmountInput_Error input_ret=e_XmountInput_Error_None;
  te_XmountMorphError morph_ret=e_XmountMorphError_None;
  te_XmountOutputError output_ret=e_XmountOutputError_None;

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
        input_ret=XmountInput_EnableDebugging(glob_xmount.h_input);
        if(input_ret!=e_XmountInput_Error_None) {
          LOG_ERROR("Unable to enable input debugging: Error code %u!\n",
                    input_ret);
          return FALSE;
        }
        morph_ret=XmountMorphing_EnableDebugging(glob_xmount.h_morphing);
        if(morph_ret!=e_XmountMorphError_None) {
          LOG_ERROR("Unable to enable morphing debugging: Error code %u!\n",
                    morph_ret);
          return FALSE;
        }
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
          LOG_ERROR("Couldn't parse fuse mount options!\n");
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
          glob_xmount.args.writable=TRUE;
        } else {
          LOG_ERROR("You must specify a cache file!\n");
          return FALSE;
        }
        LOG_DEBUG("Enabling virtual write support using cache file \"%s\"\n",
                  glob_xmount.args.p_cache_file);
      } else if(strcmp(pp_argv[i],"--in")==0) {
        // Input image format and source files
        if((i+2)<argc) {
          i++;
          // Save format
          p_buf=pp_argv[i];
          // Parse input image filename(s) and save to temporary array
          i++;
          buf=0;
          pp_buf=NULL;
          while(i<(argc-1) && strncmp(pp_argv[i],"--",2)!=0) {
            buf++;
            XMOUNT_REALLOC(pp_buf,char**,buf*sizeof(char*));
            pp_buf[buf-1]=pp_argv[i];
            i++;
          }
          i--;
          if(buf==0) {
            LOG_ERROR("No input files specified for \"--in %s\"!\n",p_buf);
            return FALSE;
          }
          // Add input image
          if((input_ret=XmountInput_AddImage(glob_xmount.h_input,
                                             p_buf,
                                             buf,
                                             (const char**)pp_buf)
             )!=e_XmountInput_Error_None)
          {
            LOG_ERROR("Unable to load input image: Error code %u!\n",input_ret);
            XMOUNT_FREE(pp_buf);
            return FALSE;
          }
          // Save first image path to generate fsname later on
          if(glob_xmount.p_first_input_image_name==NULL) {
            XMOUNT_STRSET(glob_xmount.p_first_input_image_name,
                          pp_buf[0]);
          }
        } else {
          LOG_ERROR("You must specify an input image format and source file!\n");
          return FALSE;
        }
      } else if(strcmp(pp_argv[i],"--inopts")==0) {
        // Set input lib options
        if((i+1)<argc) {
          i++;
          input_ret=XmountInput_SetOptions(glob_xmount.h_input,pp_argv[i]);
          if(input_ret!=e_XmountInput_Error_None) {
            LOG_ERROR("Unable to parse input library options: Error code %u!\n",
                      input_ret);
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
          morph_ret=XmountMorphing_SetType(glob_xmount.h_morphing,
                                           pp_argv[i]);
          if(morph_ret!=e_XmountMorphError_None) {
            LOG_ERROR("Unable to set morphing type: Error code %u!\n",
                      morph_ret);
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
          morph_ret=XmountMorphing_SetOptions(glob_xmount.h_morphing,
                                              pp_argv[i]);
          if(morph_ret!=e_XmountMorphError_None) {
            LOG_ERROR("Unable to parse morphing library options: "
                        "Error code %u!\n",
                      morph_ret);
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
          buf=StrToUint64(pp_argv[i],&ret);
          if(ret==0) {
            LOG_ERROR("Unable to convert '%s' to a number!\n",pp_argv[i]);
            return FALSE;
          }
          input_ret=XmountInput_SetInputOffset(glob_xmount.h_input,buf);
          if(input_ret!=e_XmountInput_Error_None) {
            LOG_ERROR("Unable to set input offset: Error code %u!\n",input_ret);
            return FALSE;
          }
        } else {
          LOG_ERROR("You must specify an offset!\n");
          return FALSE;
        }
      } else if(strcmp(pp_argv[i],"--out")==0) {
        // Set output lib to use
        if((i+1)<argc) {
          i++;
          output_ret=XmountOutput_SetFormat(glob_xmount.h_output,
                                            pp_argv[i]);
          if(output_ret!=e_XmountOutputError_None) {
            LOG_ERROR("Unable to set output format: Error code %u!\n",
                      output_ret);
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
          output_ret=XmountOutput_SetOptions(glob_xmount.h_output,
                                             pp_argv[i]);
          if(output_ret!=e_XmountOutputError_None) {
            LOG_ERROR("Unable to parse output library options: "
                        "Error code %u!\n",
                      output_ret);
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
          glob_xmount.args.writable=TRUE;
          glob_xmount.args.overwrite_cache=TRUE;
        } else {
          LOG_ERROR("You must specify a cache file!\n");
          return FALSE;
        }
        LOG_DEBUG("Enabling virtual write support overwriting cache file %s\n",
                  glob_xmount.args.p_cache_file);
      } else if(strcmp(pp_argv[i],"--sizelimit")==0) {
        // Set input image size limit
        if((i+1)<argc) {
          i++;
          buf=StrToUint64(pp_argv[i],&ret);
          if(ret==0) {
            LOG_ERROR("Unable to convert '%s' to a number!\n",pp_argv[i]);
            return FALSE;
          }
          input_ret=XmountInput_SetInputSizeLimit(glob_xmount.h_input,buf);
          if(input_ret!=e_XmountInput_Error_None) {
            LOG_ERROR("Unable to set input size limit: Error code %u!\n",
                      input_ret);
            return FALSE;
          }
        } else {
          LOG_ERROR("You must specify a size limit!\n");
          return FALSE;
        }
      } else if(strcmp(pp_argv[i],"--version")==0 ||
                strcmp(pp_argv[i],"--info")==0)
      {
        // Print xmount info
        printf(XMOUNT_COPYRIGHT_NOTICE "\n\n",XMOUNT_VERSION);
#ifdef __GNUC__
        printf("  compile timestamp: %s %s\n",__DATE__,__TIME__);
        printf("  gcc version: %s\n",__VERSION__);
#endif

#define PARSECMDLINE__PRINT_LOADED_LIBINFO(text,libret,fun,handle,err_ok) do { \
  printf(text);                                                                \
  libret=fun(handle,&p_buf);                                                   \
  if(p_buf==NULL || libret!=err_ok) {                                          \
    LOG_ERROR("Unable to get library infos: Error code %u!\n",libret);         \
    return FALSE;                                                              \
  }                                                                            \
  printf("%s",p_buf);                                                          \
  XMOUNT_FREE(p_buf);                                                          \
} while(0)

        PARSECMDLINE__PRINT_LOADED_LIBINFO("  loaded input libraries:\n",
                                           input_ret,
                                           XmountInput_GetLibsInfoText,
                                           glob_xmount.h_input,
                                           e_XmountInput_Error_None);
        PARSECMDLINE__PRINT_LOADED_LIBINFO("  loaded morphing libraries:\n",
                                           morph_ret,
                                           XmountMorphing_GetLibsInfoText,
                                           glob_xmount.h_morphing,
                                           e_XmountMorphError_None);
        PARSECMDLINE__PRINT_LOADED_LIBINFO("  loaded output libraries:\n",
                                           output_ret,
                                           XmountOutput_GetLibsInfoText,
                                           glob_xmount.h_output,
                                           e_XmountOutputError_None);

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
    LOG_ERROR("No mountpoint specified!\n");
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
    if(glob_xmount.p_first_input_image_name!=NULL) {
      // Set name of first source file as fsname
      XMOUNT_STRAPP(glob_xmount.pp_fuse_argv[glob_xmount.fuse_argc-1],
                    ",fsname='");
      // If possible, use full path
      p_buf=realpath(glob_xmount.p_first_input_image_name,NULL);
      if(p_buf==NULL) {
        XMOUNT_STRSET(p_buf,glob_xmount.p_first_input_image_name);
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

#undef PARSECMDLINE__PRINT_LOADED_LIBINFO

  return TRUE;
}

//! Extract output file name from input image name
/*!
 * \param p_orig_name Name of input image (may include a path)
 * \return TRUE on success, FALSE on error
 */
static int ExtractOutputFileNames(char *p_orig_name) {
  // TODO: Reimplement in output lib
/*
  char *tmp;

  // Truncate any leading path
  tmp=strrchr(p_orig_name,'/');
  if(tmp!=NULL) p_orig_name=tmp+1;

  // Extract file extension
  tmp=strrchr(p_orig_name,'.');

  // Set leading '/'
  XMOUNT_STRSET(glob_xmount.output.p_virtual_image_path,"/");
  XMOUNT_STRSET(glob_xmount.p_info_path,"/");

  // Copy filename
  if(tmp==NULL) {
    // Input image filename has no extension
    XMOUNT_STRAPP(glob_xmount.output.p_virtual_image_path,p_orig_name);
    XMOUNT_STRAPP(glob_xmount.p_info_path,p_orig_name);
    XMOUNT_STRAPP(glob_xmount.p_info_path,".info");
  } else {
    XMOUNT_STRNAPP(glob_xmount.output.p_virtual_image_path,p_orig_name,
                   strlen(p_orig_name)-strlen(tmp));
    XMOUNT_STRNAPP(glob_xmount.p_info_path,p_orig_name,
                   strlen(p_orig_name)-strlen(tmp));
    XMOUNT_STRAPP(glob_xmount.p_info_path,".info");
  }

  // Add virtual file extensions
  // TODO: Get from output lib and add
  //XMOUNT_STRAPP(glob_xmount.output.p_virtual_image_path,".dd");

  LOG_DEBUG("Set virtual image name to \"%s\"\n",
            glob_xmount.output.p_virtual_image_path);
  LOG_DEBUG("Set virtual image info name to \"%s\"\n",
            glob_xmount.p_info_path);
*/
  return FALSE;
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
  // TODO: Reimplement
/*
  char hash[16];
  md5_state_t md5_state;
  char *p_buf;
  int ret;
  size_t read_data;

  XMOUNT_MALLOC(p_buf,char*,HASH_AMOUNT*sizeof(char));
  ret=ReadMorphedImageData(p_buf,0,HASH_AMOUNT,&read_data);
  if(ret!=TRUE || read_data==0) {
    LOG_ERROR("Couldn't read data from morphed image file!\n");
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
*/
  return TRUE;
}

//! Create info file
/*!
 * \return TRUE on success, FALSE on error
 */
static int InitInfoFile() {
  char *p_buf;
  te_XmountInput_Error input_ret=e_XmountInput_Error_None;
  te_XmountMorphError morph_ret=e_XmountMorphError_None;

  // Start with static input header
  XMOUNT_MALLOC(glob_xmount.p_info_file,
                char*,
                strlen(IMAGE_INFO_INPUT_HEADER)+1);
  strncpy(glob_xmount.p_info_file,
          IMAGE_INFO_INPUT_HEADER,
          strlen(IMAGE_INFO_INPUT_HEADER)+1);

#define INITINFOFILE__GET_CONTENT(libret,fun,handle,err_ok,err_msg) do { \
  libret=fun(handle,&p_buf);                                             \
  if(p_buf==NULL || libret!=err_ok) {                                    \
    LOG_ERROR(err_msg "Error code %u!\n",libret);                        \
    return FALSE;                                                        \
  }                                                                      \
  XMOUNT_STRAPP(glob_xmount.p_info_file,p_buf);                   \
  XMOUNT_FREE(p_buf);                                                    \
} while(0)

  // Get and add infos from input lib(s)
  INITINFOFILE__GET_CONTENT(input_ret,
                            XmountInput_GetInfoFileContent,
                            glob_xmount.h_input,
                            e_XmountInput_Error_None,
                            "Unable to get info file content from input lib: ");

  // Add static morphing header
  XMOUNT_STRAPP(glob_xmount.p_info_file,IMAGE_INFO_MORPHING_HEADER);

  // Get and add infos from morphing lib
  INITINFOFILE__GET_CONTENT(morph_ret,
                            XmountMorphing_GetInfoFileContent,
                            glob_xmount.h_morphing,
                            e_XmountMorphError_None,
                            "Unable to get info file content from morphing "
                              "lib: ");

#undef INITINFOFILE__GET_CONTENT

  return TRUE;
}

//! Load input / morphing libs
/*!
 * \return TRUE on success, FALSE on error
 */
static int LoadLibs() {
  DIR *p_dir=NULL;
  struct dirent *p_dirent=NULL;
  uint32_t input_lib_count=0;
  uint32_t morphing_lib_count=0;
  uint32_t output_lib_count=0;
  te_XmountInput_Error input_ret=e_XmountInput_Error_None;
  te_XmountMorphError morph_ret=e_XmountMorphError_None;
  te_XmountOutputError output_ret=e_XmountOutputError_None;

  LOG_DEBUG("Searching for xmount libraries in '%s'.\n",
            XMOUNT_LIBRARY_PATH);

  // Open lib dir
  p_dir=opendir(XMOUNT_LIBRARY_PATH);
  if(p_dir==NULL) {
    LOG_ERROR("Unable to access xmount library directory '%s'!\n",
              XMOUNT_LIBRARY_PATH);
    return FALSE;
  }

  // Loop over lib dir
  while((p_dirent=readdir(p_dir))!=NULL) {
    LOG_DEBUG("Trying to load '%s'\n",p_dirent->d_name);

    if(strncmp(p_dirent->d_name,
               XMOUNT_INPUT_LIBRARY_NAMING_SCHEME,
               strlen(XMOUNT_INPUT_LIBRARY_NAMING_SCHEME))==0)
    {
      // Found possible input lib. Try to load it
      input_ret=XmountInput_AddLibrary(glob_xmount.h_input,p_dirent->d_name);
      if(input_ret!=e_XmountInput_Error_None) {
        LOG_ERROR("Unable to add input library '%s': Error code %u!\n",
                  p_dirent->d_name,
                  input_ret);
        continue;
      }
    } if(strncmp(p_dirent->d_name,
                 XMOUNT_MORPHING_LIBRARY_NAMING_SCHEME,
                 strlen(XMOUNT_MORPHING_LIBRARY_NAMING_SCHEME))==0)
    {
      // Found possible morphing lib. Try to load it
      morph_ret=XmountMorphing_AddLibrary(glob_xmount.h_morphing,
                                          p_dirent->d_name);
      if(morph_ret!=e_XmountMorphError_None) {
        LOG_ERROR("Unable to add morphing library '%s': Error code %u!\n",
                  p_dirent->d_name,
                  morph_ret);
        continue;
      }
    } if(strncmp(p_dirent->d_name,"libxmount_output_",17)==0) {
      // Found possible output lib. Try to load it
      output_ret=XmountOutput_AddLibrary(glob_xmount.h_output,
                                         p_dirent->d_name);
      if(output_ret!=e_XmountOutputError_None) {
        LOG_ERROR("Unable to add output library '%s': Error code %u!\n",
                  p_dirent->d_name,
                  output_ret);
        continue;
      }
    } else {
      LOG_DEBUG("Ignoring '%s'.\n",p_dirent->d_name);
      continue;
    }
  }

#undef LIBXMOUNT_LOAD_SYMBOL
#undef LIBXMOUNT_LOAD

  // Get loaded library counts
  input_ret=XmountInput_GetLibraryCount(glob_xmount.h_input,&input_lib_count);
  if(input_ret!=e_XmountInput_Error_None) {
    LOG_ERROR("Unable to get input library count: Error code %u!\n",input_ret);
    return FALSE;
  }
  morph_ret=XmountMorphing_GetLibraryCount(glob_xmount.h_morphing,
                                           &morphing_lib_count);
  if(morph_ret!=e_XmountMorphError_None) {
    LOG_ERROR("Unable to get morphing library count: Error code %u!\n",
              morph_ret);
    return FALSE;
  }
  output_ret=XmountOutput_GetLibraryCount(glob_xmount.h_output,
                                          &output_lib_count);
  if(output_ret!=e_XmountOutputError_None) {
    LOG_ERROR("Unable to get output library count: Error code %u!\n",
              output_ret);
    return FALSE;
  }

  LOG_DEBUG("A total of %u input libs, %u morphing libs and %u output libs "
              "were loaded.\n",
            input_lib_count,
            morphing_lib_count,
            output_lib_count);

  closedir(p_dir);
  return ((input_lib_count>0 &&
           morphing_lib_count>0 &&
           output_lib_count>0) ? TRUE : FALSE);
}

static int InitResources() {
  te_XmountInput_Error input_ret=e_XmountInput_Error_None;
  te_XmountMorphError morph_ret=e_XmountMorphError_None;
  te_XmountOutputError output_ret=e_XmountOutputError_None;

  // Args
  glob_xmount.args.overwrite_cache=FALSE;
  glob_xmount.args.p_cache_file=NULL;
  glob_xmount.args.writable=FALSE;

  // Input
  input_ret=XmountInput_CreateHandle(&(glob_xmount.h_input));
  if(input_ret!=e_XmountInput_Error_None) {
    LOG_ERROR("Unable to create input handle: Error code %u!\n",input_ret);
    return FALSE;
  }

  // Morphing
  morph_ret=XmountMorphing_CreateHandle(&(glob_xmount.h_morphing),
                                        &LibXmount_Morphing_ImageCount,
                                        &LibXmount_Morphing_Size,
                                        &LibXmount_Morphing_Read,
                                        &LibXmount_Morphing_Write);
  if(morph_ret!=e_XmountMorphError_None) {
    LOG_ERROR("Unable to create morphing handle: Error code %u!\n",morph_ret);
    return FALSE;
  }

  // Cache
  glob_xmount.h_cache=NULL;

  // Output
  output_ret=XmountOutput_CreateHandle(&(glob_xmount.h_output),
                                       &LibXmount_Output_Size,
                                       &LibXmount_Output_Read,
                                       &LibXmount_Output_Write);
  if(output_ret!=e_XmountOutputError_None) {
    LOG_ERROR("Unable to create output handle: Error code %u!\n",output_ret);
    return FALSE;
  }

  // Misc data
  glob_xmount.debug=FALSE;
  glob_xmount.may_set_fuse_allow_other=FALSE;
  glob_xmount.fuse_argc=0;
  glob_xmount.pp_fuse_argv=NULL;
  glob_xmount.p_mountpoint=NULL;
  glob_xmount.p_first_input_image_name=NULL;
  glob_xmount.p_info_path=NULL;
  glob_xmount.p_info_file=NULL;
  glob_xmount.image_hash_lo=0;
  glob_xmount.image_hash_hi=0;

  return TRUE;
}

/*
 * FreeResources
 */
static void FreeResources() {
  int ret;
  te_XmountInput_Error input_ret=e_XmountInput_Error_None;
  te_XmountMorphError morph_ret=e_XmountMorphError_None;
  te_XmountCache_Error cache_ret=e_XmountCache_Error_None;
  te_XmountOutputError output_ret=e_XmountOutputError_None;

  LOG_DEBUG("Freeing all resources\n");

  // Misc
  if(glob_xmount.p_first_input_image_name!=NULL) {
    XMOUNT_FREE(glob_xmount.p_first_input_image_name);
  }
  if(glob_xmount.pp_fuse_argv!=NULL) {
    for(int i=0;i<glob_xmount.fuse_argc;i++) {
      XMOUNT_FREE(glob_xmount.pp_fuse_argv[i]);
    }
    XMOUNT_FREE(glob_xmount.pp_fuse_argv);
  }
  if(glob_xmount.p_mountpoint!=NULL) XMOUNT_FREE(glob_xmount.p_mountpoint);
  if(glob_xmount.p_info_path!=NULL) XMOUNT_FREE(glob_xmount.p_info_path);
  if(glob_xmount.p_info_file!=NULL) XMOUNT_FREE(glob_xmount.p_info_file);

  // Output
  output_ret=XmountOutput_DestroyHandle(&(glob_xmount.h_output));
  if(output_ret!=e_XmountOutputError_None) {
    LOG_ERROR("Unable to destroy output handle: Error code %u: Ignoring!\n",
              output_ret);
  }

  // Cache
  if(glob_xmount.h_cache!=NULL) {
    cache_ret=XmountCache_Close(&(glob_xmount.h_cache));
    if(cache_ret!=e_XmountCache_Error_None) {
      LOG_ERROR("Unable to close cache file: Error code %u: Ignoring!\n",
                cache_ret);
    }
  }

  // Morphing
  morph_ret=XmountMorphing_DestroyHandle(&(glob_xmount.h_morphing));
  if(morph_ret!=e_XmountMorphError_None) {
    LOG_ERROR("Unable to destroy morphing handle: Error code %u: Ignoring!\n",
              morph_ret);
  }

  // Input
  // Just in case close was not already called, call it now
  input_ret=XmountInput_Close(glob_xmount.h_input);
  if(input_ret!=e_XmountInput_Error_None) {
    LOG_ERROR("Unable to close input image(s): Error code %u: Ignoring!\n",
              input_ret);
  }
  input_ret=XmountInput_DestroyHandle(&(glob_xmount.h_input));
  if(input_ret!=e_XmountInput_Error_None) {
    LOG_ERROR("Unable to destroy input handle: Error code %u: Ignoring!\n",
              input_ret);
  }

  // Args
  if(glob_xmount.args.p_cache_file!=NULL) {
    XMOUNT_FREE(glob_xmount.args.p_cache_file);
  }

  // Before we return, initialize everything in case ReleaseResources would be
  // called again.
  // TODO: This is bad as it will re-create handles etc... Get rid of it!!
  InitResources();
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
  te_XmountInput_Error input_ret=e_XmountInput_Error_None;

  input_ret=XmountInput_GetImageCount(glob_xmount.h_input,p_count);
  if(input_ret!=e_XmountInput_Error_None) {
    LOG_ERROR("Unable to get input image count: Error code %u!\n",input_ret);
    return -EIO;
  }

  return 0;
}

//! Function to get the size of the morphed data
/*!
 * \param image Image number
 * \param p_size Pointer to store input image's size to
 * \return 0 on success
 */
static int LibXmount_Morphing_Size(uint64_t image, uint64_t *p_size) {
  te_XmountInput_Error input_ret=e_XmountInput_Error_None;

  input_ret=XmountInput_GetSize(glob_xmount.h_input,image,p_size);
  if(input_ret!=e_XmountInput_Error_None) {
    LOG_ERROR("Unable to get size of input image %" PRIu64 ": Error code %u!\n",
              image,
              input_ret);
    return -EIO;
  }

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
  te_XmountInput_Error input_ret=e_XmountInput_Error_None;

  input_ret=XmountInput_ReadData(glob_xmount.h_input,
                                 image,
                                 p_buf,
                                 offset,
                                 count,
                                 p_read);
  if(input_ret!=e_XmountInput_Error_None) {
    LOG_ERROR("Unable to read data of input image %" PRIu64
                ": Error code %u!\n",
              image,
              input_ret);
    return -EIO;
  }

  return 0;
}

static int LibXmount_Morphing_Write(uint64_t image,
                                    char *p_buf,
                                    off_t offset,
                                    size_t count,
                                    size_t *p_written)
{
  // TODO: Implement
/*
  te_XmountInput_Error input_ret=e_XmountInput_Error_None;

  input_ret=XmountInput_ReadData(glob_xmount.h_input,
                                 image,
                                 p_buf,
                                 offset,
                                 count,
                                 p_read);
  if(input_ret!=e_XmountInput_Error_None) {
    LOG_ERROR("Unable to read data of input image %" PRIu64
                ": Error code %u!\n",
              image,
              input_ret);
    return -EIO;
  }
*/
  return 0;
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
  te_XmountMorphError morph_ret=e_XmountMorphError_None;

  morph_ret=XmountMorphing_GetSize(glob_xmount.h_morphing,p_size);
  if(morph_ret!=e_XmountMorphError_None) {
    LOG_ERROR("Unable to get morphed image size: Error code %u!\n",morph_ret);
    return FALSE;
  }

  return TRUE;
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
  uint64_t block_off=0;
  uint64_t cur_block=0;
  uint64_t cur_to_read=0;
  uint64_t image_size=0;
  size_t read=0;
  size_t to_read=0;
  uint8_t is_block_cached=FALSE;
  te_XmountMorphError morph_ret=e_XmountMorphError_None;
  te_XmountCache_Error cache_ret=e_XmountCache_Error_None;

  // Make sure we aren't reading past EOF of image file
  morph_ret=XmountMorphing_GetSize(glob_xmount.h_morphing,&image_size);
  if(morph_ret!=e_XmountMorphError_None) {
    LOG_ERROR("Couldn't get size of morphed image: Error code %u!\n",morph_ret);
    return -EIO;
  }
  if(offset>=image_size) {
    // Offset is beyond image size
    LOG_DEBUG("Offset %zu is at / beyond size of morphed image.\n",offset);
    *p_read=0;
    return FALSE;
  }
  if(offset+count>image_size) {
    // Attempt to read data past EOF of morphed image file
    to_read=image_size-offset;
    LOG_DEBUG("Attempt to read data past EOF of morphed image. Corrected size "
                "from %zu to %" PRIu64 ".\n",
              count,
              to_read);
  } else to_read=count;

  // Calculate block to start reading data from
  cur_block=offset/XMOUNT_CACHE_BLOCK_SIZE;
  block_off=offset%XMOUNT_CACHE_BLOCK_SIZE;

  // Read image data
  while(to_read!=0) {
    // Calculate how many bytes we have to read from this block
    if(block_off+to_read>XMOUNT_CACHE_BLOCK_SIZE) {
      cur_to_read=XMOUNT_CACHE_BLOCK_SIZE-block_off;
    } else cur_to_read=to_read;

    // Determine if we have to read cached data
    is_block_cached=FALSE;
    if(glob_xmount.args.writable==TRUE) {
      cache_ret=XmountCache_IsBlockCached(glob_xmount.h_cache,cur_block);
      if(cache_ret==e_XmountCache_Error_None) is_block_cached=TRUE;
      else if(cache_ret!=e_XmountCache_Error_UncachedBlock) {
        LOG_ERROR("Unable to determine if block %" PRIu64 " is cached: "
                    "Error code %u!\n",
                  cur_block,
                  cache_ret);
        return -EIO;
      }
    }

    // Check if block is cached
    if(is_block_cached==TRUE) {
      // Write support enabled and need to read altered data from cachefile
      LOG_DEBUG("Reading %zu bytes from block cache file\n",cur_to_read);

      cache_ret=XmountCache_BlockCacheRead(glob_xmount.h_cache,
                                           p_buf,
                                           cur_block,
                                           block_off,
                                           cur_to_read);
      if(cache_ret!=e_XmountCache_Error_None) {
        LOG_ERROR("Unable to read %" PRIu64
                    " bytes of cached data from cache block %" PRIu64
                    " at cache block offset %" PRIu64 ": Error code %u!\n",
                  cur_to_read,
                  cur_block,
                  block_off,
                  cache_ret);
        return -EIO;
      }
    } else {
      // No write support or data not cached
      morph_ret=XmountMorphing_ReadData(glob_xmount.h_morphing,
                                        p_buf,
                                        (cur_block*XMOUNT_CACHE_BLOCK_SIZE)+
                                          block_off,
                                        cur_to_read,
                                        &read);
      if(morph_ret!=e_XmountMorphError_None || read!=cur_to_read) {
        LOG_ERROR("Couldn't read %zu bytes at offset %zu from morphed image: "
                    "Error code %u!\n",
                  cur_to_read,
                  offset,
                  morph_ret);
        return -EIO;
      }
      LOG_DEBUG("Read %" PRIu64 " bytes at offset %" PRIu64
                  " from morphed image file\n",
                cur_to_read,
                (cur_block*XMOUNT_CACHE_BLOCK_SIZE)+block_off);
    }

    cur_block++;
    block_off=0;
    p_buf+=cur_to_read;
    to_read-=cur_to_read;
  }

  *p_read=to_read;
  return TRUE;
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
  uint64_t block_off=0;
  uint64_t cur_block=0;
  uint64_t cur_to_read=0;
  uint64_t cur_to_write=0;
  uint64_t image_size=0;
  uint64_t read=0;
  size_t to_write=0;
  char *p_buf2=NULL;
  uint8_t is_block_cached=FALSE;
  te_XmountMorphError morph_ret=e_XmountMorphError_None;
  te_XmountCache_Error cache_ret=e_XmountCache_Error_None;

  // Make sure we aren't writing past EOF of image file
  morph_ret=XmountMorphing_GetSize(glob_xmount.h_morphing,&image_size);
  if(morph_ret!=e_XmountMorphError_None) {
    LOG_ERROR("Couldn't get size of morphed image: Error code %u!\n",morph_ret);
    return -EIO;
  }
  if(offset>=image_size) {
    // Offset is beyond image size
    LOG_DEBUG("Offset %zu is at / beyond size of morphed image.\n",offset);
    *p_written=0;
    return 0;
  }
  if(offset+count>image_size) {
    // Attempt to write data past EOF of morphed image file
    to_write=image_size-offset;
    LOG_DEBUG("Attempt to write data past EOF of morphed image. Corrected size "
                "from %zu to %" PRIu64 ".\n",
              count,
              to_write);
  } else to_write=count;

  // Calculate block to start writing data to
  cur_block=offset/XMOUNT_CACHE_BLOCK_SIZE;
  block_off=offset%XMOUNT_CACHE_BLOCK_SIZE;

  while(to_write!=0) {
    // Calculate how many bytes we have to write to this block
    if(block_off+to_write>XMOUNT_CACHE_BLOCK_SIZE) {
      cur_to_write=XMOUNT_CACHE_BLOCK_SIZE-block_off;
    } else cur_to_write=to_write;

    // Determine if block is already in cache
    is_block_cached=FALSE;
    cache_ret=XmountCache_IsBlockCached(glob_xmount.h_cache,cur_block);
    if(cache_ret==e_XmountCache_Error_None) is_block_cached=TRUE;
    else if(cache_ret!=e_XmountCache_Error_UncachedBlock) {
      LOG_ERROR("Unable to determine if block %" PRIu64 " is cached: "
                  "Error code %u!\n",
                cur_block,
                cache_ret);
      return -EIO;
    }

    // Check if block is cached
    if(is_block_cached==TRUE) {
      // Block is cached
      cache_ret=XmountCache_BlockCacheWrite(glob_xmount.h_cache,
                                            p_buf,
                                            cur_block,
                                            block_off,
                                            cur_to_write);
      if(cache_ret!=e_XmountCache_Error_None) {
        LOG_ERROR("Unable to write %" PRIu64
                    " bytes of data to cache block %" PRIu64
                    " at cache block offset %" PRIu64 ": Error code %u!\n",
                  cur_to_write,
                  cur_block,
                  block_off,
                  cache_ret);
        return -EIO;
      }

      LOG_DEBUG("Wrote %" PRIu64 " bytes to block cache\n",cur_to_write);
    } else {
      // Uncached block. Need to cache entire new block
      // Prepare new write buffer
      XMOUNT_MALLOC(p_buf2,char*,XMOUNT_CACHE_BLOCK_SIZE);
      memset(p_buf2,0x00,XMOUNT_CACHE_BLOCK_SIZE);

      // Read full block from morphed image
      cur_to_read=XMOUNT_CACHE_BLOCK_SIZE;
      if((cur_block*XMOUNT_CACHE_BLOCK_SIZE)+XMOUNT_CACHE_BLOCK_SIZE>image_size) {
        cur_to_read=XMOUNT_CACHE_BLOCK_SIZE-(((cur_block*XMOUNT_CACHE_BLOCK_SIZE)+
                                         XMOUNT_CACHE_BLOCK_SIZE)-image_size);
      }
      morph_ret=XmountMorphing_ReadData(glob_xmount.h_morphing,
                                        p_buf,
                                        cur_block*XMOUNT_CACHE_BLOCK_SIZE,
                                        cur_to_read,
                                        &read);
      if(morph_ret!=e_XmountMorphError_None || read!=cur_to_read) {
        LOG_ERROR("Couldn't read %" PRIu64 " bytes at offset %zu "
                    "from morphed image: Error code %u!\n",
                  cur_to_read,
                  offset,
                  morph_ret);
        XMOUNT_FREE(p_buf2);
        return -EIO;
      }

      // Set changed data
      memcpy(p_buf2+block_off,p_buf,cur_to_write);

      cache_ret=XmountCache_BlockCacheAppend(glob_xmount.h_cache,
                                             p_buf,
                                             cur_block);
      if(cache_ret!=e_XmountCache_Error_None) {
        LOG_ERROR("Unable to append new block cache block %" PRIu64
                    ": Error code %u!\n",
                  cur_block,
                  cache_ret);
        XMOUNT_FREE(p_buf2);
        return -EIO;
      }
      XMOUNT_FREE(p_buf2);

      LOG_DEBUG("Appended new block cache block %" PRIu64 "\n",cur_block);
    }

    block_off=0;
    cur_block++;
    p_buf+=cur_to_write;
    to_write-=cur_to_write;
  }

  *p_written=to_write;
  return TRUE;
}

/*******************************************************************************
 * Main
 ******************************************************************************/
int main(int argc, char *argv[]) {
  uint64_t input_image_count=0;
  struct stat file_stat;
  int ret;
  int fuse_ret;
  char *p_err_msg;
  te_XmountInput_Error input_ret=e_XmountInput_Error_None;
  te_XmountMorphError morph_ret=e_XmountMorphError_None;
  //te_XmountCache_Error cache_ret=e_XmountCache_Error_None;
  te_XmountOutputError output_ret=e_XmountOutputError_None;

  // Set implemented FUSE functions
  struct fuse_operations xmount_operations = {
    // File functions
    .create=XmountFuse_create,
    .ftruncate=XmountFuse_ftruncate,
    .open=XmountFuse_open,
    .read=XmountFuse_read,
    .release=XmountFuse_release,
    .write=XmountFuse_write,
    // Dir functions
    .mkdir=XmountFuse_mkdir,
    .readdir=XmountFuse_readdir,
    // Misc functions
    .chmod=XmountFuse_chmod,
    .chown=XmountFuse_chown,
    .getattr=XmountFuse_getattr,
    .link=XmountFuse_link,
    .readlink=XmountFuse_readlink,
    .rename=XmountFuse_rename,
    .rmdir=XmountFuse_rmdir,
    //.statfs=XmountFuse_statfs,
    .symlink=XmountFuse_symlink,
    .truncate=XmountFuse_truncate,
    .utimens=XmountFuse_utimens,
    .unlink=XmountFuse_unlink
/*
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
*/
  };

  // Disable std output / input buffering
  setbuf(stdout,NULL);
  setbuf(stderr,NULL);

  // Init glob_xmount
  if(InitResources()==FALSE) {
    LOG_ERROR("Unable to initialize internal resources!\n");
    return 1;
  }

  // Load input, morphing and output libs
  if(!LoadLibs()) {
    LOG_ERROR("Unable to load any libraries!\n");
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
  input_ret=XmountInput_GetImageCount(glob_xmount.h_input,&input_image_count);
  if(input_ret!=e_XmountInput_Error_None) {
    LOG_ERROR("Unable to get input image count: Error code %u!\n",input_ret);
    return 1;
  }
  if(input_image_count==0) {
    LOG_ERROR("No --in command line option specified!\n");
    PrintUsage(argv[0]);
    FreeResources();
    return 1;
  }
  if(glob_xmount.fuse_argc<2) {
    LOG_ERROR("Couldn't parse command line options!\n");
    PrintUsage(argv[0]);
    FreeResources();
    return 1;
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
    LOG_DEBUG("Options passed to FUSE: ");
    for(int i=0;i<glob_xmount.fuse_argc;i++) {
      printf("%s ",glob_xmount.pp_fuse_argv[i]);
    }
    printf("\n");
  }

  // Init mutexes
  pthread_mutex_init(&(glob_xmount.mutex_image_rw),NULL);
  pthread_mutex_init(&(glob_xmount.mutex_info_read),NULL);

  // Open input image(s)
  input_ret=XmountInput_Open(glob_xmount.h_input);
  if(input_ret!=e_XmountInput_Error_None) {
    LOG_ERROR("Failed opening input image(s): Error code %u!\n",input_ret);
    FreeResources();
    return 1;
  }

  // Morph input image(s)
  morph_ret=XmountMorphing_StartMorphing(glob_xmount.h_morphing);
  if(morph_ret!=e_XmountMorphError_None) {
    LOG_ERROR("Unable to start morphing: Error code %u!\n",morph_ret);
    FreeResources();
    return 1;
  }

  // Open / Create cache if needed
  if(glob_xmount.args.writable) {
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
      LOG_ERROR("Couldn't initialize cache file!\n");
      FreeResources();
      return 1;
    }
*/
    LOG_DEBUG("Cache file initialized successfully\n");
  }

  // Init random generator
  srand(time(NULL));

  // Calculate partial MD5 hash of input image file
  if(CalculateInputImageHash(&(glob_xmount.image_hash_lo),
                             &(glob_xmount.image_hash_hi))==FALSE)
  {
    LOG_ERROR("Couldn't calculate partial hash of morphed image!\n");
    return 1;
  }

  if(glob_xmount.debug==TRUE) {
    LOG_DEBUG("Partial MD5 hash of morphed image: ");
    for(int i=0;i<8;i++)
      printf("%02hhx",*(((char*)(&(glob_xmount.image_hash_lo)))+i));
    for(int i=0;i<8;i++)
      printf("%02hhx",*(((char*)(&(glob_xmount.image_hash_hi)))+i));
    printf("\n");
  }

  // Transform morphed image into output format
  output_ret=XmountOutput_Transform(glob_xmount.h_output);
  if(output_ret!=e_XmountOutputError_None) {
    LOG_ERROR("Unable to transform output image: Error code %u!\n",output_ret);
    FreeResources();
    return 1;
  }

  // Gather infos for info file
  if(!InitInfoFile()) {
    LOG_ERROR("Couldn't gather infos for virtual image info file!\n");
    FreeResources();
    return 1;
  }
  LOG_DEBUG("Virtual image info file build successfully\n");

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
