if HAVE_MPI
EXEMPI = ltsmin-mpi
if HAVE_LIBMCRL
EXEMPI += instantiator-mpi
endif
LIBMPI = libmpi.a
endif

bin_PROGRAMS = ltsmin-convert $(EXEMPI)
noinst_LIBRARIES = $(LIBMPI) libutil.a

# instantiator-mpi
instantiator_mpi_SOURCES = instantiator-mpi.c config.h
instantiator_mpi_LDADD = libmpi.a libutil.a

# ltsmin-mpi
ltsmin_mpi_SOURCES = ltsmin-mpi.c set.c lts.c dlts.c set.h lts.h	\
    dlts.h config.h
ltsmin_mpi_LDADD = libmpi.a libutil.a

# ltsmin-convert
ltsmin_convert_SOURCES = ltsmin-convert.c
ltsmin_convert_LDADD = libutil.a

# libraries
libmpi_a_SOURCES = mpi_io_stream.c mpi_core.c mpi_raf.c mpi_ram_raf.c	\
	 mpi_core.h mpi_io_stream.h mpi_ram_raf.h config.h

libutil_a_SOURCES = unix.c runtime.c stream.c archive.c ltsmeta.c	\
                    stream_buffer.c fast_hash.c generichash4.c		\
                    generichash8.c treedbs.c archive_format.c raf.c	\
                    stream_mem.c ghf.c archive_gcf.c scctimer.c		\
                    gzstream.c stream_diff32.c options.c arch_object.h	\
                    generichash.h unix.h archive.h ghf.h options.h	\
                    stream.h raf.h stream_object.h ltsmeta.h		\
                    raf_object.h scctimer.h fast_hash.h runtime.h	\
                    treedbs.h config.h

# make 
.DELETE_ON_ERROR: