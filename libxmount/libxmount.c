/*******************************************************************************
* xmount Copyright (c) 2008-2016 by Gillen Daniel <gillen.dan@pinguin.lu>      *
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

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <errno.h>
#include <stdarg.h>
#include <string.h>

#include "libxmount.h"
#include "../src/macros.h"

/*
 * XmountLib_SplitLibParams
 */
int XmountLib_SplitLibParams(char *p_params,
                             uint32_t *p_ret_opts_count,
                             pts_LibXmountOptions **ppp_ret_opt)
{
  pts_LibXmountOptions p_opts=NULL;
  pts_LibXmountOptions *pp_opts=NULL;
  uint32_t params_len;
  uint32_t opts_count=0;
  uint32_t sep_pos=0;
  char *p_buf=p_params;

  if(p_params==NULL) return 0;

  // Get params length
  params_len=strlen(p_params);

  // Return if no params specified
  if(params_len==0) {
    *ppp_ret_opt=NULL;
    p_ret_opts_count=0;
    return 1;
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
      return 0;
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
      return 0;
    }

    // Save option value
    XMOUNT_STRNSET(p_opts->p_value,p_buf,sep_pos);
    p_buf+=sep_pos;

    LIBXMOUNT_LOG_DEBUG("Extracted library option: '%s' = '%s'\n",
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

  LIBXMOUNT_LOG_DEBUG("Extracted a total of %" PRIu32 " library options\n",
                      opts_count);

  *p_ret_opts_count=opts_count;
  *ppp_ret_opt=pp_opts;
  return 1;
}

/*
 * LogMessage
 */
void LogMessage(char *p_msg_type,
                char *p_calling_fun,
                int line,
                char *p_msg,
                ...)
{
  va_list var_list;

  // Print message "header"
  printf("%s: %s@%u : ",p_msg_type,p_calling_fun,line);
  // Print message with variable parameters
  va_start(var_list,p_msg);
  vprintf(p_msg,var_list);
  va_end(var_list);
}

/*
 * StrToInt32
 */
int32_t StrToInt32(const char *p_value, int *p_ok) {
  long int num;
  char *p_tail;

  errno=0;
  num=strtol(p_value,&p_tail,0);
  if(errno==ERANGE || *p_tail!='\0' || num<INT32_MIN || num>INT32_MAX) {
    *p_ok=0;
    return 0;
  }

  *p_ok=1;
  return (int32_t)num;
}

/*
 * StrToUint32
 */
uint32_t StrToUint32(const char *p_value, int *p_ok) {
  unsigned long int num;
  char *p_tail;

  errno=0;
  num=strtoul(p_value,&p_tail,0);
  if(errno==ERANGE || *p_tail!='\0' || num>UINT32_MAX) {
    *p_ok=0;
    return 0;
  }

  *p_ok=1;
  return (uint32_t)num;
}

/*
 * StrToInt64
 */
int64_t StrToInt64(const char *p_value, int *p_ok) {
  long long int num;
  char *p_tail;

  errno=0;
  num=strtoll(p_value,&p_tail,0);
  if(errno==ERANGE || *p_tail!='\0' || num<INT64_MIN || num>INT64_MAX) {
    *p_ok=0;
    return 0;
  }

  *p_ok=1;
  return (int64_t)num;
}

/*
 * StrToUint64
 */
uint64_t StrToUint64(const char *p_value, int *p_ok) {
  unsigned long long int num;
  char *p_tail;

  errno=0;
  num=strtoull(p_value,&p_tail,0);
  if(errno==ERANGE || *p_tail!='\0' || num>UINT64_MAX) {
    *p_ok=0;
    return 0;
  }

  *p_ok=1;
  return (uint64_t)num;
}

