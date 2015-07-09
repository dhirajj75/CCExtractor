#include "lib_ccx.h"
#include "ccx_common_option.h"
#include "activity.h"
#include "ccx_demuxer.h"

unsigned char tspacket[188]; // Current packet

struct ts_payload payload;
unsigned char *last_pat_payload=NULL;
unsigned last_pat_length = 0;


static unsigned char *haup_capbuf = NULL;
static long haup_capbufsize = 0;
static long haup_capbuflen = 0; // Bytes read in haup_capbuf

// Descriptions for ts ccx_stream_type
const char *desc[256];

char *get_buffer_type_str(struct cap_info *cinfo)
{
	if( cinfo->stream == CCX_STREAM_TYPE_VIDEO_MPEG2)
	{
		return strdup("MPG");
	}
	else if( cinfo->stream == CCX_STREAM_TYPE_VIDEO_H264 )
	{
		return strdup("H.264");
	}
	else if ( cinfo->stream == CCX_STREAM_TYPE_PRIVATE_MPEG2 && cinfo->codec == CCX_CODEC_DVB )
	{
		return strdup("DVB subtitle");
	}
	else if ( cinfo->stream == CCX_STREAM_TYPE_UNKNOWNSTREAM && ccx_options.hauppauge_mode)
	{
		return strdup("Hauppage");
	}
	else if ( cinfo->stream == CCX_STREAM_TYPE_PRIVATE_MPEG2 && cinfo->codec == CCX_CODEC_TELETEXT)
	{
		return strdup("Teletext");
	}
	else if ( cinfo->stream == CCX_STREAM_TYPE_PRIVATE_MPEG2 && cinfo->codec == CCX_CODEC_ATSC_CC)
	{
		return strdup("CC in private MPEG packet");
	}
	else
	{
		return NULL;
	}
}
enum ccx_stream_type get_buffer_type(struct cap_info *cinfo)
{
	if( cinfo->stream == CCX_STREAM_TYPE_VIDEO_MPEG2)
	{
		return CCX_PES;
	}
	else if( cinfo->stream == CCX_STREAM_TYPE_VIDEO_H264 )
	{
		return CCX_H264;
	}
	else if ( cinfo->stream == CCX_STREAM_TYPE_PRIVATE_MPEG2 && cinfo->codec == CCX_CODEC_DVB )
	{
		return CCX_DVB_SUBTITLE;
	}
	else if ( cinfo->stream == CCX_STREAM_TYPE_UNKNOWNSTREAM && ccx_options.hauppauge_mode)
	{
		return CCX_HAUPPAGE;
	}
	else if ( cinfo->stream == CCX_STREAM_TYPE_PRIVATE_MPEG2 && cinfo->codec == CCX_CODEC_TELETEXT)
	{
		return CCX_TELETEXT;
	}
	else if ( cinfo->stream == CCX_STREAM_TYPE_PRIVATE_MPEG2 && cinfo->codec == CCX_CODEC_ATSC_CC)
	{
		return CCX_PRIVATE_MPEG2_CC;
	}
	else
	{
		return CCX_EINVAL;
	}
}
void init_ts(struct ccx_demuxer *ctx)
{
	// Constants
	desc[CCX_STREAM_TYPE_UNKNOWNSTREAM] = "Unknown";
	desc[CCX_STREAM_TYPE_VIDEO_MPEG1] = "MPEG-1 video";
	desc[CCX_STREAM_TYPE_VIDEO_MPEG2] = "MPEG-2 video";
	desc[CCX_STREAM_TYPE_AUDIO_MPEG1] = "MPEG-1 audio";
	desc[CCX_STREAM_TYPE_AUDIO_MPEG2] = "MPEG-2 audio";
	desc[CCX_STREAM_TYPE_MHEG_PACKETS] = "MHEG Packets";
	desc[CCX_STREAM_TYPE_PRIVATE_TABLE_MPEG2] = "MPEG-2 private table sections";
	desc[CCX_STREAM_TYPE_PRIVATE_MPEG2] ="MPEG-2 private data";
	desc[CCX_STREAM_TYPE_MPEG2_ANNEX_A_DSM_CC] = "MPEG-2 Annex A DSM CC";
	desc[CCX_STREAM_TYPE_ITU_T_H222_1] = "ITU-T Rec. H.222.1";
	desc[CCX_STREAM_TYPE_AUDIO_AAC] =   "AAC audio";
	desc[CCX_STREAM_TYPE_VIDEO_MPEG4] = "MPEG-4 video";
	desc[CCX_STREAM_TYPE_VIDEO_H264] =  "H.264 video";
	desc[CCX_STREAM_TYPE_PRIVATE_USER_MPEG2] = "MPEG-2 User Private";
	desc[CCX_STREAM_TYPE_AUDIO_AC3] =   "AC3 audio";
	desc[CCX_STREAM_TYPE_AUDIO_DTS] =   "DTS audio";
	desc[CCX_STREAM_TYPE_AUDIO_HDMV_DTS]="HDMV audio";
	desc[CCX_STREAM_TYPE_ISO_IEC_13818_6_TYPE_A] = "ISO/IEC 13818-6 type A";
	desc[CCX_STREAM_TYPE_ISO_IEC_13818_6_TYPE_B] = "ISO/IEC 13818-6 type B";
	desc[CCX_STREAM_TYPE_ISO_IEC_13818_6_TYPE_C] = "ISO/IEC 13818-6 type C";
	desc[CCX_STREAM_TYPE_ISO_IEC_13818_6_TYPE_D] = "ISO/IEC 13818-6 type D";
}


// Return 1 for sucessfully read ts packet
int ts_readpacket(struct ccx_demuxer* ctx)
{
	if (ctx->m2ts)
	{
		/* M2TS just adds 4 bytes to each packet (so size goes from 188 to 192)
		   The 4 bytes are not important to us, so just skip
		// TP_extra_header {   
		Copy_permission_indicator 2  unimsbf
		Arrival_time_stamp 30 unimsbf
		} */
		unsigned char tp_extra_header[4];
		buffered_read(ctx, tp_extra_header, 3);
		ctx->past += result;
		if (result != 4)
		{
			if (result>0)
				mprint("Premature end of file!\n");
			return CCX_EOF;
		}
	}
	buffered_read(ctx, tspacket, 188);
	ctx->past+=result;
	if (result!=188)
	{
		if (result>0)
			mprint("Premature end of file!\n");
		return CCX_EOF;
	}

	int printtsprob = 1;
	while (tspacket[0]!=0x47)
	{
		if (printtsprob)
		{
			mprint ("\nProblem: No TS header mark (filepos=%lld). Received bytes:\n", ctx->past);
			dump (CCX_DMT_GENERIC_NOTICES, tspacket,4, 0, 0);

			mprint ("Skip forward to the next TS header mark.\n");
			printtsprob = 0;
		}

		unsigned char *tstemp;
		// The amount of bytes read into tspacket
		int tslen = 188;

		// Check for 0x47 in the remaining bytes of tspacket
		tstemp = (unsigned char *) memchr (tspacket+1, 0x47, tslen-1);
		if (tstemp != NULL )
		{
			// Found it
			int atpos = tstemp-tspacket;

			memmove (tspacket,tstemp,(size_t)(tslen-atpos));
			buffered_read(ctx, tspacket+(tslen-atpos), atpos);
			ctx->past+=result;
			if (result!=atpos)
			{
				mprint("Premature end of file!\n");
				return CCX_EOF;
			}
		}
		else
		{
			// Read the next 188 bytes.
			buffered_read(ctx, tspacket, tslen);
			ctx->past+=result;
			if (result!=tslen)
			{
				mprint("Premature end of file!\n");
				return CCX_EOF;
			}
		}
	}

	unsigned char *payload_start = tspacket + 4;
	unsigned payload_length = 188 - 4;

	unsigned transport_error_indicator = (tspacket[1]&0x80)>>7;
	unsigned payload_start_indicator = (tspacket[1]&0x40)>>6;
	// unsigned transport_priority = (tspacket[1]&0x20)>>5;
	unsigned pid = (((tspacket[1] & 0x1F) << 8) | tspacket[2]) & 0x1FFF;
	// unsigned transport_scrambling_control = (tspacket[3]&0xC0)>>6;
	unsigned adaptation_field_control = (tspacket[3]&0x30)>>4;
	unsigned ccounter = tspacket[3] & 0xF;

	if (transport_error_indicator)
	{
		mprint ("Warning: Defective (error indicator on) TS packet (filepos=%lld):\n", ctx->past);
		dump (CCX_DMT_GENERIC_NOTICES, tspacket, 188, 0, 0);
	}

	unsigned adaptation_field_length = 0;
	if ( adaptation_field_control & 2 )
	{
		// Take the PCR (Program Clock Reference) from here, in case PTS is not available (copied from telxcc).
		adaptation_field_length = tspacket[4];

		uint8_t af_pcr_exists = (tspacket[5] & 0x10) >> 4;
		if (af_pcr_exists > 0)
		{
			uint64_t pts = 0;
			pts |= (tspacket[6] << 25);
			pts |= (tspacket[7] << 17);
			pts |= (tspacket[8] << 9);
			pts |= (tspacket[9] << 1);
			pts |= (tspacket[10] >> 7);
			ctx->global_timestamp = (uint32_t) pts / 90;
			pts = ((tspacket[10] & 0x01) << 8);
			pts |= tspacket[11];
			ctx->global_timestamp += (uint32_t) (pts / 27000);
			if (!ctx->global_timestamp_inited)
			{
				ctx->min_global_timestamp = ctx->global_timestamp;
				ctx->global_timestamp_inited = 1;
			}
		}


		payload_start = payload_start + adaptation_field_length + 1;
		payload_length = tspacket+188-payload_start;
	}

	dbg_print(CCX_DMT_PARSE, "TS pid: %d  PES start: %d  counter: %u  payload length: %u  adapt length: %d\n",
			pid, payload_start_indicator, ccounter, payload_length,
			(int) (adaptation_field_length));

	// Catch bad packages with adaptation_field_length > 184 and
	// the unsigned nature of payload_length leading to huge numbers.
	if (payload_length > 184)
	{
		// This renders the package invalid
		payload_length = 0;
		dbg_print(CCX_DMT_PARSE, "  Reject package - set length to zero.\n");
	}

	// Save data in global struct
	payload.start = payload_start;
	payload.length = payload_length;
	payload.pesstart = payload_start_indicator;
	payload.pid = pid;
	payload.counter = ccounter;
	payload.transport_error = transport_error_indicator;
	if (payload_length == 0)
	{
		dbg_print(CCX_DMT_PARSE, "  No payload in package.\n");
	}
	// Store packet information
	return CCX_OK;
}



void look_for_caption_data (struct ccx_demuxer *ctx)
{
	// See if we find the usual CC data marker (GA94) in this packet.
	if (payload.length<4 || ctx->PIDs_seen[payload.pid]==3) // Second thing means we already inspected this PID
		return;
	for (unsigned i=0;i<payload.length-3;i++)
	{
		if (payload.start[i]=='G' && payload.start[i+1]=='A' &&
				payload.start[i+2]=='9' && payload.start[i+3]=='4')
		{
			mprint ("PID %u seems to contain captions.\n", payload.pid);
			ctx->PIDs_seen[payload.pid]=3;
			return;
		}
	}

}

struct demuxer_data *search_or_alloc_demuxer_data_node_by_pid(struct demuxer_data **data, int pid)
{
	struct demuxer_data *ptr;
	struct demuxer_data *sptr;
	if (!*data)
	{
		*data = alloc_demuxer_data();
		(*data)->program_number = -1;
		(*data)->stream_pid = pid;
		(*data)->bufferdatatype = CCX_UNKNOWN;
		(*data)->len = 0;
		(*data)->next_program = NULL;
		(*data)->next_stream = NULL;
		(*data)->ignore = 0;
		return *data;
	}
	ptr = *data;

	do
	{
		if(ptr->stream_pid == pid)
		{
			return ptr;
		}
		sptr = ptr;
		ptr = ptr->next_stream;
	} while(ptr);

	sptr->next_stream = alloc_demuxer_data();
	ptr = sptr->next_stream;
	ptr->program_number = -1;
	ptr->stream_pid = pid;
	ptr->bufferdatatype = CCX_UNKNOWN;
	ptr->len = 0;
	ptr->next_program = NULL;
	ptr->next_stream = NULL;
	ptr->ignore = 0;

	return ptr;
}

int get_best_stream_id(struct demuxer_data *data)
{
	//int i;
	//ctx->cinfo[ctx->nb_cap]	
	return -1;
}
struct demuxer_data *get_best_data(struct demuxer_data *data)
{
	struct demuxer_data *ret = NULL;
	struct demuxer_data *ptr = data;
	for(ptr = data; ptr; ptr = ptr->next_stream)
	{
		if(ptr->codec == CCX_CODEC_TELETEXT)
		{
			ret =  data;
			goto end;
		}
	}

	for(ptr = data; ptr; ptr = ptr->next_stream)
	{
		if(ptr->codec == CCX_CODEC_DVB)
		{
			ret =  data;
			goto end;
		}
	}

	for(ptr = data; ptr; ptr = ptr->next_stream)
	{
		if(ptr->codec == CCX_CODEC_ATSC_CC)
		{
			ret =  ptr;
			goto end;
		}
	}
end:
	for(ptr = data; ptr && ret; ptr = ptr->next_stream)
	{
		if(ptr->stream_pid != ret->stream_pid)
			ptr->ignore = 1;
	}

	return ret;
}

int copy_capbuf_demux_data(struct ccx_demuxer *ctx, struct demuxer_data **data, struct cap_info *cinfo)
{
	int vpesdatalen;
	int pesheaderlen;
	unsigned char *databuf;
	long databuflen;
	struct demuxer_data *ptr;

	ptr = search_or_alloc_demuxer_data_node_by_pid(data, cinfo->pid);
	ptr->program_number = cinfo->program_number;
	ptr->codec = cinfo->codec;
	ptr->bufferdatatype = get_buffer_type(cinfo);

	if(ptr->ignore)
	{
		cinfo->ignore = 1;
		return CCX_OK;
	}

	if (ptr->bufferdatatype == CCX_PRIVATE_MPEG2_CC)
	{
		dump (CCX_DMT_GENERIC_NOTICES, cinfo->capbuf, cinfo->capbuflen, 0, 1);
		// Bogus data, so we return something
		ptr->buffer[ptr->len++] = 0xFA;
		ptr->buffer[ptr->len++] = 0x80;
		ptr->buffer[ptr->len++] = 0x80;
		return CCX_OK;
	}
	if (cinfo->codec == CCX_CODEC_TELETEXT)
	{
		memcpy(ptr->buffer+ ptr->len, cinfo->capbuf, cinfo->capbuflen);
		ptr->len = cinfo->capbuflen + ptr->len;
		ptr->len += cinfo->capbuflen;
		return CCX_OK;
	}
	vpesdatalen = read_video_pes_header(ctx, cinfo->capbuf, &pesheaderlen, cinfo->capbuflen);
	if (vpesdatalen < 0)
	{
		dbg_print(CCX_DMT_VERBOSE, "Seems to be a broken PES. Terminating file handling.\n");
		return CCX_EOF;
	}

	if (ccx_options.hauppauge_mode)
	{
		if (haup_capbuflen%12 != 0)
			mprint ("Warning: Inconsistent Hauppage's buffer length\n");
		if (!haup_capbuflen)
		{
			// Do this so that we always return something until EOF. This will be skipped.
			ptr->buffer[ptr->len++] = 0xFA;
			ptr->buffer[ptr->len++] = 0x80;
			ptr->buffer[ptr->len++] = 0x80;
		}

		for (int i = 0; i<haup_capbuflen; i += 12)
		{
			unsigned haup_stream_id = haup_capbuf[i+3];
			if (haup_stream_id == 0xbd && haup_capbuf[i+4] == 0 && haup_capbuf[i+5] == 6 )
			{
				// Because I (CFS) don't have a lot of samples for this, for now I make sure everything is like the one I have:
				// 12 bytes total length, stream id = 0xbd (Private non-video and non-audio), etc
				if (2 > BUFSIZE - ptr->len)
				{
					fatal(CCX_COMMON_EXIT_BUG_BUG,
							"Remaining buffer (%lld) not enough to hold the 3 Hauppage bytes.\n"
							"Please send bug report!",
							BUFSIZE - ptr->len);
				}
				if (haup_capbuf[i+9]==1 || haup_capbuf[i+9]==2) // Field match. // TODO: If extract==12 this won't work!
				{
					if (haup_capbuf[i+9]==1)
						ptr->buffer[ptr->len++]=4; // Field 1 + cc_valid=1
					else
						ptr->buffer[ptr->len++]=5; // Field 2 + cc_valid=1
					ptr->buffer[ptr->len++]=haup_capbuf[i+10];
					ptr->buffer[ptr->len++]=haup_capbuf[i+11];
				}
				/*
				   if (inbuf>1024) // Just a way to send the bytes to the decoder from time to time, otherwise the buffer will fill up.
				   break;
				   else
				   continue; */
			}
		}
		haup_capbuflen=0;
	}
	databuf = cinfo->capbuf + pesheaderlen;
	databuflen = cinfo->capbuflen - pesheaderlen;

	if (!ccx_options.hauppauge_mode) // in Haup mode the buffer is filled somewhere else
	{
		if(ptr->len + databuflen >= BUFSIZE)
		{
			fatal(CCX_COMMON_EXIT_BUG_BUG,
				"PES data packet (%ld) larger than remaining buffer (%lld).\n"
				"Please send bug report!",
				databuflen, BUFSIZE - ptr->len);
			return CCX_EAGAIN;
		}
		memcpy(ptr->buffer+ ptr->len, databuf, databuflen);
		ptr->len += databuflen;
	}
	return CCX_OK;
}

void cinfo_cremation(struct ccx_demuxer *ctx, struct demuxer_data **data)
{
	struct cap_info* iter; 
	list_for_each_entry(iter ,&ctx->cinfo_tree.all_stream, all_stream)
	{
		copy_capbuf_demux_data(ctx, data, iter);
		freep(&iter->capbuf);
	}
}

int copy_payload_to_capbuf(struct cap_info *cinfo, struct ts_payload *payload)
{
	int newcapbuflen;
	//Verify PES before copy to capbuf
	if(cinfo->capbuflen == 0 )
	{
		if(payload->start[0] != 0x00 || payload->start[1] != 0x00 || 
			payload->start[2] != 0x01)
		{
			mprint("Missing PES header!\n");
			dump(CCX_DMT_GENERIC_NOTICES, payload->start, payload->length, 0, 0);
			cinfo->saw_pesstart = 0;
			errno = EINVAL;
			return -1;
		}
	}

	// copy payload to capbuf
	newcapbuflen = cinfo->capbuflen + payload->length;
	if ( newcapbuflen > cinfo->capbufsize) {
		cinfo->capbuf = (unsigned char*)realloc(cinfo->capbuf, newcapbuflen);
		if (!cinfo->capbuf)
			return -1;
		cinfo->capbufsize = newcapbuflen;
	}
	memcpy(cinfo->capbuf + cinfo->capbuflen, payload->start, payload->length);
	cinfo->capbuflen = newcapbuflen;

	return CCX_OK;
}
// Read ts packets until a complete video PES element can be returned.
// The data is read into capbuf and the function returns the number of
// bytes read.
long ts_readstream(struct ccx_demuxer *ctx, struct demuxer_data **data)
{
	int gotpes = 0;
	long pespcount = 0; // count packets in PES with captions
	long pcount=0; // count all packets until PES is complete
	int packet_analysis_mode = 0; // If we can't find any packet with CC based from PMT, look for captions in all packets
	int ret = CCX_EAGAIN;
	struct cap_info *cinfo;

	do
	{
		pcount++;

		// Exit the loop at EOF
		ret = ts_readpacket(ctx);
		if ( ret != CCX_OK)
			break;

		// Skip damaged packets, they could do more harm than good
		if (payload.transport_error)
		{
			dbg_print(CCX_DMT_VERBOSE, "Packet (pid %u) skipped - transport error.\n",
				payload.pid);
			continue;
		}
		// Skip packets with no payload.  This also fixes the problems
		// with the continuity counter not being incremented in empty
		// packets.
		if ( !payload.length )
		{
			dbg_print(CCX_DMT_VERBOSE, "Packet (pid %u) skipped - no payload.\n",
				payload.pid);
			continue;
		}

		// Check for PAT
		if( payload.pid == 0) // This is a PAT
		{
			parse_PAT(ctx); // Returns 1 if there was some data in the buffer already
			continue;
		}
		
		if( ccx_options.xmltv >= 1 && payload.pid == 0x12) // This is DVB EIT
			parse_EPG_packet(ctx->parent);
		if( ccx_options.xmltv >= 1 && payload.pid >= 0x1000) // This may be ATSC EPG packet
			parse_EPG_packet(ctx->parent);

		// PID != 0 but no PMT selected yet, ignore the rest of the current
		// package and continue searching, UNLESS we are in -autoprogram, which requires us
		// to analyze all PMTs to look for a stream with data.
//		if ( !pmtpid && ccx_options.teletext_mode!=CCX_TXT_IN_USE && !ctx->ts_autoprogram)
//		{
//			dbg_print(CCX_DMT_PARSE, "Packet (pid %u) skipped - no PMT pid identified yet.\n",
//					   payload.pid);
//			continue;
//		}

		int is_pmt=0, j;
		for (j=0;j<ctx->pmt_array_length;j++)
		{
			if (ctx->pmt_array[j].PMT_PID==payload.pid)
			{
				if (!ctx->PIDs_seen[payload.pid])
					dbg_print(CCX_DMT_PAT, "This PID (%u) is a PMT for program %u.\n",payload.pid, ctx->pmt_array[j].program_number);
				is_pmt=1;
				break;
			}
		}

		if (is_pmt)
		{
			ctx->PIDs_seen[payload.pid]=2;
			if(payload.pesstart)
			{
				int len = *payload.start++;
				payload.start += len;
				if(write_section(ctx, &payload,payload.start,(tspacket + 188 ) - payload.start,j))
					gotpes=1; // Signals that something changed and that we must flush the buffer
			}
			else
			{
				if(write_section(ctx, &payload,payload.start,(tspacket + 188 ) - payload.start,j))
					gotpes=1; // Signals that something changed and that we must flush the buffer
			}
//			if (payload.pid==pmtpid && ctx->nb_cap == 0 && ccx_options.investigate_packets) // It was our PMT yet we don't have a PID to get data from
//				packet_analysis_mode=1;

			continue;
		}

		switch (ctx->PIDs_seen[payload.pid])
		{
			case 0: // First time we see this PID
				if (ctx->PIDs_programs[payload.pid])
				{
					dbg_print(CCX_DMT_PARSE, "\nNew PID found: %u (%s), belongs to program: %u\n", payload.pid,
						desc[ctx->PIDs_programs[payload.pid]->printable_stream_type],
						ctx->PIDs_programs[payload.pid]->program_number);
					ctx->PIDs_seen[payload.pid]=2;
				}
				else
				{
					dbg_print(CCX_DMT_PARSE, "\nNew PID found: %u, program number still unknown\n", payload.pid);
					ctx->PIDs_seen[payload.pid]=1;
				}
				break;
			case 1: // Saw it before but we didn't know what program it belonged to. Luckier now?
				if (ctx->PIDs_programs[payload.pid])
				{
					dbg_print(CCX_DMT_PARSE, "\nProgram for PID: %u (previously unknown) is: %u (%s)\n", payload.pid,
						ctx->PIDs_programs[payload.pid]->program_number,
						desc[ctx->PIDs_programs[payload.pid]->printable_stream_type]
						);
					ctx->PIDs_seen[payload.pid]=2;
				}
				break;
			case 2: // Already seen and reported with correct program
				break;
			case 3: // Already seen, reported, and inspected for CC data (and found some)
				break;
		}


		if (payload.pid==1003 && !ctx->hauppauge_warning_shown && !ccx_options.hauppauge_mode)
		{
			// TODO: Change this very weak test for something more decent such as size.
			mprint ("\n\nNote: This TS could be a recording from a Hauppage card. If no captions are detected, try --hauppauge\n\n");
			ctx->hauppauge_warning_shown=1;
		}

		if (ccx_options.hauppauge_mode && payload.pid==HAUPPAGE_CCPID)
		{
			// Haup packets processed separately, because we can't mix payloads. So they go in their own buffer
			// copy payload to capbuf
			int haup_newcapbuflen = haup_capbuflen + payload.length;
			if ( haup_newcapbuflen > haup_capbufsize) {
				haup_capbuf = (unsigned char*)realloc(haup_capbuf, haup_newcapbuflen);
				if (!haup_capbuf)
					fatal(EXIT_NOT_ENOUGH_MEMORY, "Out of memory");
				haup_capbufsize = haup_newcapbuflen;
			}
			memcpy(haup_capbuf+haup_capbuflen, payload.start, payload.length);
			haup_capbuflen = haup_newcapbuflen;

		}

		cinfo = get_cinfo(ctx, payload.pid);
		if(cinfo == NULL)
		{
			if (!packet_analysis_mode)
				dbg_print(CCX_DMT_PARSE, "Packet (pid %u) skipped - no stream with captions identified yet.\n",
					   payload.pid);
			else
				look_for_caption_data (ctx);
			continue;
		}


		// Video PES start
		if (payload.pesstart)
		{
			cinfo->saw_pesstart = 1;
			cinfo->prev_counter = payload.counter - 1;
		}

		// Discard packets when no pesstart was found.
		if ( !cinfo->saw_pesstart )
			continue;


		if ( (cinfo->prev_counter == 15 ? 0 : cinfo->prev_counter + 1) != payload.counter )
		{
			mprint("TS continuity counter not incremented prev/curr %u/%u\n",
					cinfo->prev_counter, payload.counter);
		}
		cinfo->prev_counter = payload.counter;

		// If the buffer is empty we just started this function
		if (payload.pesstart && cinfo->capbuflen > 0)
		{
			dbg_print(CCX_DMT_PARSE, "\nPES finished (%ld bytes/%ld PES packets/%ld total packets)\n",
					cinfo->capbuflen, pespcount, pcount);

			// Keep the data from capbuf to be worked on
			ret = copy_capbuf_demux_data(ctx, data, cinfo);
			cinfo->capbuflen = 0;
			gotpes = 1;
		}

		copy_payload_to_capbuf(cinfo, &payload);
		if(ret < 0)
		{
			if(errno == EINVAL)
				continue;
			else
				break;
		}

		pespcount++;
		//else
		//	if(debug_verbose)
		//		printf("Packet (pid %u) skipped - unused.\n",
		//			   payload.pid);
		// Nothing suitable found, start over
	}
	while( !gotpes ); // gotpes==1 never arrives here because of the breaks

	if (ret == CCX_EOF)
	{
		cinfo_cremation(ctx, data);
	}
	return ret;
}


// TS specific data grabber
LLONG ts_getmoredata(struct ccx_demuxer *ctx, struct demuxer_data **data)
{
//	const char *tstr; // Temporary string to describe the stream type
	int ret;

#define search_again goto search
#define done goto end
search:
	ret = ts_readstream(ctx, data);
	if(ret == CCX_EAGAIN)
		search_again;
	else if (ret == CCX_EOF)
		done;
/*
	if (ctx->ts_cap_stream_type[0] == CCX_STREAM_TYPE_UNKNOWNSTREAM && ccx_options.ts_forced_streamtype != CCX_STREAM_TYPE_UNKNOWNSTREAM)
	{
		ctx->ts_cap_stream_type[0] = ccx_options.ts_forced_streamtype;
	}
*/

#if 0
	unsigned stream_id = ctx->capbuf[3];


	dbg_print(CCX_DMT_VERBOSE, "TS payload start video PES id: %d  len: %ld\n",
			stream_id, ctx->capbuflen);

#endif
#if 0
	if (ccx_bufferdatatype == CCX_DVB_SUBTITLE && !vpesdatalen)
	{
		dbg_print(CCX_DMT_VERBOSE, "TS payload is a DVB Subtitle\n");
		payload_read = ctx->capbuflen;
		inbuf += payload_read;
		done;
	}



	// If the package length is unknown vpesdatalen is zero.
	// If we know he package length, use it to quit
	dbg_print(CCX_DMT_VERBOSE, "Read PES-%s (databuffer %ld/PES data %d) ",
			tstr, databuflen, vpesdatalen);
	// We got the whole PES in buffer
	if( vpesdatalen && (databuflen >= vpesdatalen) )
		dbg_print(CCX_DMT_VERBOSE, " - complete");
	dbg_print(CCX_DMT_VERBOSE, "\n");



#endif
end:
#undef search_again
#undef done

	return ret;
}
