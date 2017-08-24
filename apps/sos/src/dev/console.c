#include "comm/comm.h"
#include "console.h"
#include "vfs/device.h"
#include "vfs/uio.h"

struct serial_console _serial;


struct device *console_dev = NULL;
/* static char last_char = 0; */
static bool produce_char(char c)
{
    /* if (last_char == 0 ) */
    /* { */
    /*     last_char = c; */
    /* } */
    /* else */
    /* { */
    /*     assert((last_char == 'z' && c == 'a')||(last_char == (c - 1))); */
    /*     last_char = c; */
    /* } */

    if (_serial._read_buf_size == MAXIMUM_SERIAL_READ_BUFFER)
    {
        /* ERROR_DEBUG("serial exceed buffer size, starts drop char: [%c]\n", c); */
        return false;
    }
    int idx;
    if (_serial._read_buf_head + _serial._read_buf_size < MAXIMUM_SERIAL_READ_BUFFER)
    {
        idx = _serial._read_buf_head + _serial._read_buf_size;
    }
    else
    {
        idx = _serial._read_buf_head + _serial._read_buf_size - MAXIMUM_SERIAL_READ_BUFFER;
    }
    _serial._read_buffer[idx] = c;
    _serial._read_buf_size ++;
    return true;
}

/* static int consume_chars(char* buf, int buf_len) */
/* { */
/*     int ret_size = 0; */
/*     if (_serial._serial_handler->_read_buf_size == 0) */
/*     { */
/*         return 0; */
/*     } */
/*     ret_size = _serial._serial_handler->_read_buf_size > buf_len ? buf_len : _serial._serial_handler->_read_buf_size; */
/*  */
/*     if (_serial._serial_handler->_read_buf_head + ret_size < MAXIMUM_SERIAL_READ_BUFFER) */
/*     { */
/*         memcpy(buf, _serial._serial_handler->_read_buffer + _serial._read_buf_head, ret_size); */
/*     } */
/*     else */
/*     { */
/*         memcpy(buf, _serial._serial_handler->_read_buffer + _serial._read_buf_head, MAXIMUM_SERIAL_READ_BUFFER - _serial._read_buf_head); */
/*         memcpy(buf + MAXIMUM_SERIAL_READ_BUFFER - _serial._read_buf_head, _serial._read_buffer, ret_size - (MAXIMUM_SERIAL_READ_BUFFER - _serial._read_buf_head)); */
/*     } */
/*     _serial_handler._read_buf_head = (_serial_handler._read_buf_head + ret_size) % MAXIMUM_SERIAL_READ_BUFFER; */
/*     return ret_size; */
/* } */

static char comsume_one_char()
{
    P(_serial._read_sem);
    char ret = _serial._read_buffer[_serial._read_buf_head];
    _serial._read_buf_head = (_serial._read_buf_head + 1) % MAXIMUM_SERIAL_READ_BUFFER;
    _serial._read_buf_size --;
    assert(_serial._read_buf_size >= 0 && _serial._read_buf_size <= MAXIMUM_SERIAL_READ_BUFFER);
    assert(_serial._read_buf_size == _serial._read_sem->_sem_count);
    /* printf ("after consume, available chars: %d\n" , _serial._read_buf_size); */
    return ret;
}

/* static dump_to_screen= 0; */
static void handle_serial_input(struct  serial* handler, char in)
{
    /* if (dump_to_screen == ) */
    /* if (in == '#') */
    /* { */
    /*     dump_to_screen = 1; */
    /*     printf ("handle_serial_input :"); */
    /* } */
    /* if (dump_to_screen == 1 && in != '\n') */
    /* { */
    /*     printf ("%c", in); */
    /*     return; */
    /* } */
    /* if (in == '\n') */
    /* { */
    /*     printf ("\n"); */
    /*     dump_to_screen = 0; */
    /* } */
    assert(handler == _serial._serial_handler);
    if (produce_char(in))
    {
        /* printf ("available chars: %d\n" , _serial._read_buf_size); */
        V(_serial._read_sem); // V responsible for put blocking coro into pending queue.
    }
    assert(_serial._read_buf_size >= 0 && _serial._read_buf_size <= MAXIMUM_SERIAL_READ_BUFFER);
    assert(_serial._read_buf_size == _serial._read_sem->_sem_count);
    return;
}


static int con_eachopen(struct device *dev, int openflags)
{
    assert((struct serial_console*)(dev->d_data) == &_serial);
    if (openflags == O_WRONLY)
    {
        COLOR_DEBUG(DB_DEVICE, ANSI_COLOR_GREEN, "con_eachopen: %d, write only\n", openflags);
        return 0;
    }
    if (openflags == O_RDONLY || openflags == O_RDWR)
    {
        if (_serial._read_exclusive_flag)
        {
            COLOR_DEBUG(DB_DEVICE, ANSI_COLOR_GREEN,"con_eachopen: %d busy\n", openflags);
            return EBUSY;
        }
        else
        {
            COLOR_DEBUG(DB_DEVICE, ANSI_COLOR_GREEN,"con_eachopen: %d, get read perm\n", openflags);
            _serial._read_exclusive_flag = 1;
            _serial._coro = current_running_coro();
            return 0;
        }
    }
    else
    {
        return EINVAL;
    }

}

#define MAX_SENT_LEN 1200
static int do_serial_send(char* buf, int len)
{
    int ret = 0;
    int sent_len = 0;
    while (sent_len != len)
    {
        ret = serial_send(_serial._serial_handler, buf + sent_len , (len - sent_len) > MAX_SENT_LEN ? MAX_SENT_LEN: len - sent_len);
        COLOR_DEBUG(DB_DEVICE, ANSI_COLOR_GREEN, "serial_send len [%u]\n", ret);

        assert(ret > 0);
        sent_len += ret;
    }
    return sent_len;
}

static int con_io(struct device* dev, struct uio* uio)
{
    assert((struct serial_console*)(dev->d_data) == &_serial);

	int result;
	char ch;

	(void)dev;  // unused

    COLOR_DEBUG(DB_DEVICE, ANSI_COLOR_GREEN, "start con_io[%d], res len: %d\n", uio->uio_rw, uio->uio_resid);
    COLOR_DEBUG(DB_DEVICE, ANSI_COLOR_GREEN, "* current console read available chars: [%u]\n", _serial._read_buf_size);

	while (uio->uio_resid > 0)
    {
		if (uio->uio_rw==UIO_READ)
        {
			ch = comsume_one_char();
			if (ch=='\r')
            {
				ch = '\n';
			}
			result = uiomove(&ch, 1, uio);
			if (result) {
				return result;
			}
			if (ch=='\n') {
				break;
			}
		}
		else
        {
            assert(uio->uio_resid <= MAXIMUM_SERIAL_WRITE_BUFFER);
            int sent_len = uio->uio_resid;
			result = uiomove(_serial._write_buffer, MAXIMUM_SERIAL_WRITE_BUFFER, uio);
			if (result)
            {
                ERROR_DEBUG("uiomove error: %d\n", result);
				return result;
			}

            int ret = do_serial_send(_serial._write_buffer, sent_len);

            COLOR_DEBUG(DB_DEVICE, ANSI_COLOR_GREEN, "sos serial_send len [%u]\n", sent_len);

            assert(ret == sent_len);
		}
	}
    COLOR_DEBUG(DB_DEVICE, ANSI_COLOR_GREEN, "* after con io, remaining read chars [%u]\n", _serial._read_buf_size);
	return 0;

}

static int con_ioctl(struct device *dev, int op, const void* data)
{
    (void) dev;
    (void) op;
    (void) data;
    return EINVAL;
}

static int con_reclaim(struct device* dev)
{
    assert((struct serial_console*)(dev->d_data) == &_serial);
    if (current_running_coro() == _serial._coro)
    {

        COLOR_DEBUG(DB_DEVICE, ANSI_COLOR_GREEN,"con_reclaim: %p, release read on the console\n", _serial._coro);
        _serial._coro = NULL;
        _serial._read_exclusive_flag = 0;
    }
    return 0;
}

static const struct device_ops console_devops =
{
    .devop_eachopen = con_eachopen,
    .devop_io = con_io,
    .devop_ioctl = con_ioctl,
    .devop_reclaim = con_reclaim,
};

static void attach_console_to_vfs()
{
     struct device *dev = malloc(sizeof(struct device));
     console_dev = dev;
     assert(dev != NULL);
     dev->d_ops = &console_devops;
     dev->d_blocks = 0;
     dev->d_blocksize = 1;
     dev->d_data = &_serial;
     assert(0 == vfs_adddev("console", dev, 0));
}

void init_console(void)
{
    _serial._serial_handler = serial_init();
    assert(_serial._serial_handler != NULL);
    _serial._read_buf_size = 0;
    _serial._read_buf_head = 0;
    assert(0 == serial_register_handler(_serial._serial_handler, handle_serial_input));
    _serial._read_exclusive_flag = 0;
    _serial._coro = NULL;
    _serial._read_sem = sem_create("serial_sem", 0, -1);
    assert(_serial._read_sem != NULL);
    attach_console_to_vfs();
}

void destroy_console(void)
{
    sem_destroy(_serial._read_sem);
    // nothing we can do.
    return;
}

