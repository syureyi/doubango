/*
* Copyright (C) 2009 Mamadou Diop.
*
* Contact: Mamadou Diop <diopmamadou@yahoo.fr>
*	
* This file is part of Open Source Doubango Framework.
*
* DOUBANGO is free software: you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.
*	
* DOUBANGO is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU Lesser General Public License for more details.
*	
* You should have received a copy of the GNU General Public License
* along with DOUBANGO.
*
*/

/**@file tcomp_message.h
 * @brief  SIGCOMP message as per RFC 3320 subclause 7.
 *
 * @author Mamadou Diop <diopmamadou(at)yahoo.fr>
 *
 * @date Created: Sat Nov 8 16:54:58 2009 mdiop
 */
#ifndef TCOMP_MESSAGE_H
#define TCOMP_MESSAGE_H

#include "tinysigcomp_config.h"
#include "tcomp_buffer.h"
#include "tcomp_nakinfo.h"
#include "tsk_object.h"

#include <stdint.h>

#define TCOMP_MESSAGE_CREATE(input_ptr, input_size, stream)	tsk_object_new(tsk_message_def_t, input_ptr, input_size, stream)
#define TCOMP_MESSAGE_SAFE_FREE(self)						tsk_object_unref(self)

/**
* SigComp Message Format as per RFC 3320 subclause 7.
*/
typedef struct tcomp_message_s
{
	TSK_DECLARE_OBJECT;

	uint8_t headerSigComp; /**< SigComp header */

	unsigned isOK:1; /**< Message validity. */
	unsigned isNack:1; /**< If it's a NACK message. */
	unsigned stream_based:1; /**< If it's stream message (e.g. TCP) */
	const uint8_t* startPtr; /**< Message pointer. */
	size_t totalSize; /**< The total message size. */
	size_t header_size;	/**< The size of the message header */
	
	tcomp_buffer_handle_t* stateId; /**< */
	
	uint16_t bytecodes_destination; /**< */
	tcomp_buffer_handle_t* remaining_sigcomp_buffer; /**< */
	tcomp_buffer_handle_t* uploaded_UDVM_buffer; /**< */
	tcomp_buffer_handle_t* ret_feedback_buffer; /**< */
	
	tcomp_nackinfo_t* nack_info; /**< */
}
tcomp_message_t;

TINYSIGCOMP_API const void *tcomp_message_def_t;

#endif /* TCOMP_MESSAGE_H */