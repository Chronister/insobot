#ifndef INSOBOT_MODULE_MSGS_H
#define INSOBOT_MODULE_MSGS_H

//
// Explanation
//

// MOD_MSG is a macro defined in module.h for passing messages between modules.
// It has the following syntax:
// 
//   MOD_MSG(IRCCoreCtx ctx, char* msg_id, arg, callback, user_defined);
//
// The callback parameter should have the following signature:
//
//   intptr_t callback(intptr_t result, intptr_t user_defined)
//
// The user_defined parameter is the same as given in the macro, and can be
// used to store the result somewhere without using a global.
//
// This macro is just a wrapper around ctx->send_mod_msg that adds casting.
// You can use that instead if you want.

// Before this macro / func returns, the callback you passed in will be called
// immediately by any module that chooses to respond to the given msg id
// (potentially multiple times). In theory multiple different modules could
// respond, but currently it's only 1:1.
//
// If there is no module that handles the given message, or if a module that
// does handle it is not loaded, then the callback will not be called at all.
// Code written using this system shouldn't assume it'll always get a response;
// it should handle the case where the callback is not called.

// New in API_VERSION 2:
// The callbacks now return intptr_t instead of void. This can be used to return
// some data to the module that called the callback. Most will ignore it currently.

//
// Message List
// 

// [L] means the arg is a space-separated List of strings in a single char*
// [M] means the callback will be called Multiple times with different results
// [F] means the result is malloc'd and should be Freed with free()
//     * if [F] is not present, then the result probably becomes invalid
//       after the callback returns, so copy it if you need to.
//
// Custom argument / return types will be defined in this file.

//    module     |        msg id          | arg type  | result type | callback return |
// --------------+------------------------+-----------+-------------+-----------------+
// mod_alias     | "alias_exists"         | char*[2]  | bool        | unused          |
// mod_hmh       | "hmh_is_live"          | unused    | bool        | unused          |
// mod_karma     | "karma_get"            | char*     | int         | unused          |
// mod_markov    | "markov_gen"           | unused    | char* [F]   | unused          |
// mod_notes     | "note_get_stream_start"| char* [L] | time_t      | unused          |
// mod_schedule  | "sched_iter"           | char*     | SchedMsg*   | SchedIterCmd    |
// mod_schedule  | "sched_add"            | SchedMsg* | bool        | unused          |
// mod_schedule  | "sched_save"           | unused    | unused      | unused          |
// mod_twitch    | "display_name"         | char*     | char*       | unused          |
// mod_twitch    | "twitch_get_user_date" | char*     | time_t      | unused          |
// mod_twitch    | "twitch_is_live"       | char* [L] | bool        | unused          |
// mod_whitelist | "check_admin"          | char*     | bool        | unused          |
// mod_whitelist | "check_whitelist"      | char*     | bool        | unused          |

//
// Descriptions
//

// ALIAS:
//  alias_exists:
//    *result* will be true/false if any of the space-separated aliases in the *aliases* field exist.

typedef struct {
	const char* aliases;
	const char* channel;
} AliasExistsMsg;

// HMH:
//  hmh_is_live:
//    *result* will be true/false if HMH is scheduled to be airing currently.

// KARMA:
//   karma_get:
//     *result* will be the karma of the user given in *arg*

// MARKOV:
//  markov_gen:
//    *result* will be a malloc'd randomly generated sentence.
//    a max length can optionally be given in *arg*

// NOTES:
//  note_get_stream_start:
//    *result* will be the time a "NOTE(Annotator): ... start" was last issued for
//    the channel given in *arg*

// SCHEDULE:
//  sched_iter:
//    This will call the callback for every known schedule, filtered to only those with the channel
//    name specified by *arg* if non-null. *result* will be set to the next SchedMsg for each call.
//    You should return one of the SchedIterCmd values to control what happens next.
//  sched_add:
//    Adds a new schedule defined by the SchedMsg given in *arg*. The sched_id is ignored.
//  sched_save:
//    Saves changes made by calls to sched_iter or sched_save, and uploads the new data to gist.
//    *arg* is ignored, and *result* unused.

typedef enum {
	SCHED_ITER_CONTINUE, // keep iterating through schedules
	SCHED_ITER_STOP,     // stop iterating
	SCHED_ITER_DELETE,   // delete this schedule, continue to the next
} SchedIterCmd;

typedef struct {
	const char* user;
	int sched_id;
	time_t start;
	time_t end;
	const char* title;
	uint8_t repeat;
} SchedMsg;

// TWITCH:
//  display_name:
//   *result* will be the current message author's capitalized / unicode twitich name.
//   if that isn't available, then *arg* is returned in *result* as a fallback.
//  twitch_get_user_date:
//    *result* will be the epoch time that the user given in *arg* created their account.
//  twitch_is_live:
//    *result* will be true/false if any of the channels given in *arg* are live or not.

// WHITELIST:
//  check_admin:
//    *result* will be true/false if the user given in *arg* is an admin or not.
//  check_whitelist:
//    *result* will be true/false if the user given in *arg* is whitelisted or not.
//    
#endif
