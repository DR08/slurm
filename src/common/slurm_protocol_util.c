#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include <src/common/slurm_protocol_defs.h>
#include <src/common/slurm_protocol_api.h>
#include <src/common/slurm_protocol_common.h>
#include <src/common/slurm_protocol_util.h>
#include <src/common/slurm_authentication.h>
#include <src/common/log.h>

/* checks to see that the specified header was sent from a node running the same version of the protocol as the current node */
uint32_t check_header_version(header_t * header)
{
	if (header->version != SLURM_PROTOCOL_VERSION) {
		debug("Invalid Protocol Version %d", header->version);
		slurm_seterrno_ret(SLURM_PROTOCOL_VERSION_ERROR);
	}
	return SLURM_PROTOCOL_SUCCESS;
}

/* simple function to create a header, always insuring that an accurate version string is inserted */
void init_header(header_t * header, slurm_msg_type_t msg_type,
		 uint16_t flags)
{
	header->version = SLURM_PROTOCOL_VERSION;
	header->flags = flags;
	header->msg_type = msg_type;
}

void update_header(header_t * header, uint32_t cred_length,
		   uint32_t msg_length)
{
	header->cred_length = cred_length;
	header->body_length = msg_length;
}

/* checks to see that the specified header was sent from a node running the same version of the protocol as the current node */
uint32_t check_io_stream_header_version(slurm_io_stream_header_t * header)
{
	if (header->version != SLURM_PROTOCOL_VERSION) {
		debug("Invalid IO Stream Protocol Version %d ",
		      header->version);
		slurm_seterrno_ret(SLURM_PROTOCOL_IO_STREAM_VERSION_ERROR);
	}
	return SLURM_PROTOCOL_SUCCESS;
}

/* simple function to create a header, always insuring that an accurate version string is inserted */
void init_io_stream_header(slurm_io_stream_header_t * header, char *key,
			   uint32_t task_id, uint16_t type)
{
	assert(key != NULL);
	header->version = SLURM_PROTOCOL_VERSION;
	memcpy(header->key, key, SLURM_SSL_SIGNATURE_LENGTH);
	header->task_id = task_id;
	header->type = type;
}

int read_io_stream_header(slurm_io_stream_header_t * header, int fd)
{
	char buffer[sizeof(slurm_io_stream_header_t)];
	char *buf_ptr = buffer;
	int  buf_size = sizeof(slurm_io_stream_header_t);
	int  size     = sizeof(slurm_io_stream_header_t);
	int  rsize    = slurm_read_stream(fd, buffer, buf_size);
	unpack_io_stream_header(header, (void **) &buf_ptr, &size);
	return rsize;
}

int write_io_stream_header(slurm_io_stream_header_t * header, int fd)
{
	char buffer[sizeof(slurm_io_stream_header_t)];
	char *buf_ptr = buffer;
	int buf_size  = sizeof(slurm_io_stream_header_t);
	int size      = sizeof(slurm_io_stream_header_t);
	pack_io_stream_header(header, (void **) &buf_ptr, &size);
	return slurm_write_stream(fd, buffer, buf_size - size);
}

int read_io_stream_header2(slurm_io_stream_header_t * header, int fd)
{
	int rsize;

	rsize = slurm_read_stream(fd, (char *)&header->version, 
			          sizeof(header->version));
	if (rsize != sizeof(header->version)) 
		return rsize;
	header->version = ntohs(header->version);

	rsize = slurm_read_stream(fd, (char *) &header->key, sizeof(header->key));
	if (rsize != sizeof(header->key)) 
		return rsize;

	rsize = slurm_read_stream(fd, (char *) &header->task_id, 
			          sizeof(header->task_id));
	if (rsize != sizeof(header->task_id)) 
		return rsize;
	header->task_id = ntohl(header->task_id);

	rsize = slurm_read_stream(fd, (char *) &header->type, sizeof(header->type));
	if (rsize != sizeof(header->type))
		return rsize;
	header->type = ntohs(header->type);

	return SLURM_SUCCESS;
}

int write_io_stream_header2(slurm_io_stream_header_t * header, int fd)
{
	int write_size;
	slurm_io_stream_header_t header2 = *header;

	header->version = htons(header2.version);
	if ((write_size =
	     slurm_write_stream(fd, (char *) &header2.version,
				sizeof(header2.version))) !=
	    sizeof(header2.version)) {
		return write_size;
	}

	if ((write_size =
	     slurm_write_stream(fd, header2.key,
				sizeof(header2.key))) !=
	    sizeof(header2.key)) {
		return write_size;
	}

	header->task_id = htonl(header2.task_id);
	if ((write_size =
	     slurm_write_stream(fd, (char *) &header2.version,
				sizeof(header2.task_id))) !=
	    sizeof(header2.task_id)) {
		return write_size;
	}

	header->type = htons(header2.type);
	if ((write_size =
	     slurm_write_stream(fd, (char *) &header2.version,
				sizeof(header2.type))) !=
	    sizeof(header2.type)) {
		return write_size;
	}

	return SLURM_SUCCESS;
}

void slurm_print_job_credential(FILE * stream,
				slurm_job_credential_t * credential)
{
	debug3("credential.job_id: %i", credential->job_id);
	debug3("credential.user_id: %i", credential->user_id);
	debug3("credential.node_list: %s", credential->node_list);
	debug3("credential.expiration_time: %lu",
	       credential->expiration_time);
	debug3("credential.signature: %#x", credential->signature);
}

void slurm_print_launch_task_msg(launch_tasks_request_msg_t * msg)
{
	int i;
	char addrbuf[256];
	debug3("job_id: %i", msg->job_id);
	debug3("job_step_id: %i", msg->job_step_id);
	debug3("uid: %i", msg->uid);
	slurm_print_job_credential(stderr, msg->credential);
	debug3("tasks_to_launch: %i", msg->tasks_to_launch);
	debug3("envc: %i", msg->envc);
	for (i = 0; i < msg->envc; i++) {
		debug3("env[%i]: %s", i, msg->env[i]);
	}
	debug3("cwd: %s", msg->cwd);
	debug3("argc: %i", msg->argc);
	for (i = 0; i < msg->argc; i++) {
		debug3("argv[%i]: %s", i, msg->argv[i]);
	}
	slurm_print_slurm_addr(&msg->response_addr, addrbuf,
			       sizeof(addrbuf));
	debug3("msg -> response_addr = %s", addrbuf);
	slurm_print_slurm_addr(&msg->streams, addrbuf, sizeof(addrbuf));
	debug3("msg -> streams = %s", addrbuf);

	for (i = 0; i < msg->tasks_to_launch; i++) {
		debug3("global_task_id[%i]: %i ", i,
		       msg->global_task_ids[i]);
	}
}
