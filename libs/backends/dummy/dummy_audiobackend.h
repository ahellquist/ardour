/*
 * Copyright (C) 2014 Robin Gareus <robin@gareus.org>
 * Copyright (C) 2013 Paul Davis
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifndef __libbackend_dummy_audiobackend_h__
#define __libbackend_dummy_audiobackend_h__

#include <string>
#include <vector>
#include <map>
#include <set>

#include <stdint.h>
#include <pthread.h>

#include <boost/shared_ptr.hpp>

#include "ardour/types.h"
#include "ardour/audio_backend.h"

namespace ARDOUR {

class DummyMidiEvent {
	public:
		DummyMidiEvent (const pframes_t timestamp, const uint8_t* data, size_t size);
		DummyMidiEvent (const DummyMidiEvent& other);
		~DummyMidiEvent ();
		size_t size () const { return _size; };
		pframes_t timestamp () const { return _timestamp; };
		const unsigned char* const_data () const { return _data; };
		unsigned char* data () { return _data; };
		bool operator< (const DummyMidiEvent &other) const { return timestamp () < other.timestamp (); };
	private:
		size_t _size;
		pframes_t _timestamp;
		uint8_t *_data;
};

typedef std::vector<boost::shared_ptr<DummyMidiEvent> > DummyMidiBuffer;

class DummyPort {
	protected:
		DummyPort (const std::string&, PortFlags);
	public:
		virtual ~DummyPort ();

		const std::string& name () const { return _name; }
		PortFlags flags () const { return _flags; }

		int set_name (const std::string &name) { _name = name; return 0; }

		virtual DataType type () const = 0;

		bool is_input ()     const { return flags () & IsInput; }
		bool is_output ()    const { return flags () & IsOutput; }
		bool is_physical ()  const { return flags () & IsPhysical; }
		bool is_terminal ()  const { return flags () & IsTerminal; }
		bool is_connected () const { return _connections.size () != 0; }
		bool is_connected (const DummyPort *port) const;
		bool is_physically_connected () const;

		const std::vector<DummyPort *>& get_connections () const { return _connections; }

		int connect (DummyPort *port);
		int disconnect (DummyPort *port);
		void disconnect_all ();

		virtual void* get_buffer (pframes_t nframes) = 0;

		const LatencyRange& latency_range (bool for_playback) const
		{
			return for_playback ? _playback_latency_range : _capture_latency_range;
		}

		void set_latency_range (const LatencyRange &latency_range, bool for_playback)
		{
			if (for_playback)
			{
				_playback_latency_range = latency_range;
			}
			else
			{
				_capture_latency_range = latency_range;
			}
		}

	private:
		std::string _name;
		const PortFlags _flags;
		LatencyRange _capture_latency_range;
		LatencyRange _playback_latency_range;
		std::vector<DummyPort*> _connections;

		void _connect (DummyPort* , bool);
		void _disconnect (DummyPort* , bool);

}; // class DummyPort

class DummyAudioPort : public DummyPort {
	public:
		DummyAudioPort (const std::string&, PortFlags);
		~DummyAudioPort ();

		DataType type () const { return DataType::AUDIO; };

		Sample* buffer () { return _buffer; }
		const Sample* const_buffer () const { return _buffer; }
		void* get_buffer (pframes_t nframes);

	private:
		Sample _buffer[8192];
}; // class DummyAudioPort

class DummyMidiPort : public DummyPort {
	public:
		DummyMidiPort (const std::string&, PortFlags);
		~DummyMidiPort ();

		DataType type () const { return DataType::MIDI; };

		void* get_buffer (pframes_t nframes);
		const DummyMidiBuffer const_buffer () const { return _buffer; }

	private:
		DummyMidiBuffer _buffer;
}; // class DummyMidiPort

class DummyAudioBackend : public AudioBackend {
	public:
	         DummyAudioBackend (AudioEngine& e, AudioBackendInfo& info);
		~DummyAudioBackend ();

		/* AUDIOBACKEND API */

		std::string name () const;
		bool is_realtime () const;

		std::vector<DeviceStatus> enumerate_devices () const;
		std::vector<float> available_sample_rates (const std::string& device) const;
		std::vector<uint32_t> available_buffer_sizes (const std::string& device) const;
		uint32_t available_input_channel_count (const std::string& device) const;
		uint32_t available_output_channel_count (const std::string& device) const;

		bool can_change_sample_rate_when_running () const;
		bool can_change_buffer_size_when_running () const;

		int set_device_name (const std::string&);
		int set_sample_rate (float);
		int set_buffer_size (uint32_t);
		int set_interleaved (bool yn);
		int set_input_channels (uint32_t);
		int set_output_channels (uint32_t);
		int set_systemic_input_latency (uint32_t);
		int set_systemic_output_latency (uint32_t);

		/* Retrieving parameters */
		std::string  device_name () const;
		float        sample_rate () const;
		uint32_t     buffer_size () const;
		bool         interleaved () const;
		uint32_t     input_channels () const;
		uint32_t     output_channels () const;
		uint32_t     systemic_input_latency () const;
		uint32_t     systemic_output_latency () const;

		/* External control app */
		std::string control_app_name () const { return std::string (); }
		void launch_control_app () {}

		/* MIDI */
		std::vector<std::string> enumerate_midi_options () const;
		int set_midi_option (const std::string&);
		std::string midi_option () const;

		/* State Control */
	protected:
		int _start (bool for_latency_measurement);
	public:
		int stop ();
		int freewheel (bool);
		float dsp_load () const;
		size_t raw_buffer_size (DataType t);

		/* Process time */
		pframes_t sample_time ();
		pframes_t sample_time_at_cycle_start ();
		pframes_t samples_since_cycle_start ();

		int create_process_thread (boost::function<void()> func);
		int join_process_threads ();
		bool in_process_thread ();
		uint32_t process_thread_count ();

		void update_latencies ();

		/* PORTENGINE API */

		void* private_handle () const;
		const std::string& my_name () const;
		bool available () const;
		uint32_t port_name_size () const;

		int         set_port_name (PortHandle, const std::string&);
		std::string get_port_name (PortHandle) const;
		PortHandle  get_port_by_name (const std::string&) const;

		int get_ports (const std::string& port_name_pattern, DataType type, PortFlags flags, std::vector<std::string>&) const;

		DataType port_data_type (PortHandle) const;

		PortHandle register_port (const std::string& shortname, ARDOUR::DataType, ARDOUR::PortFlags);
		void unregister_port (PortHandle);

		int  connect (const std::string& src, const std::string& dst);
		int  disconnect (const std::string& src, const std::string& dst);
		int  connect (PortHandle, const std::string&);
		int  disconnect (PortHandle, const std::string&);
		int  disconnect_all (PortHandle);

		bool connected (PortHandle, bool process_callback_safe);
		bool connected_to (PortHandle, const std::string&, bool process_callback_safe);
		bool physically_connected (PortHandle, bool process_callback_safe);
		int  get_connections (PortHandle, std::vector<std::string>&, bool process_callback_safe);

		/* MIDI */
		int midi_event_get (pframes_t& timestamp, size_t& size, uint8_t** buf, void* port_buffer, uint32_t event_index);
		int midi_event_put (void* port_buffer, pframes_t timestamp, const uint8_t* buffer, size_t size);
		uint32_t get_midi_event_count (void* port_buffer);
		void     midi_clear (void* port_buffer);

		/* Monitoring */

		bool can_monitor_input () const;
		int  request_input_monitoring (PortHandle, bool);
		int  ensure_input_monitoring (PortHandle, bool);
		bool monitoring_input (PortHandle);

		/* Latency management */

		void         set_latency_range (PortHandle, bool for_playback, LatencyRange);
		LatencyRange get_latency_range (PortHandle, bool for_playback);

		/* Discovering physical ports */

		bool      port_is_physical (PortHandle) const;
		void      get_physical_outputs (DataType type, std::vector<std::string>&);
		void      get_physical_inputs (DataType type, std::vector<std::string>&);
		ChanCount n_physical_outputs () const;
		ChanCount n_physical_inputs () const;

		/* Getting access to the data buffer for a port */

		void* get_buffer (PortHandle, pframes_t);

		void* main_process_thread ();

	private:
		std::string _instance_name;
		bool  _running;
		bool  _freewheeling;

		float  _samplerate;
		size_t _samples_per_period;
		float  _dsp_load;
		static size_t _max_buffer_size;

		uint32_t _n_inputs;
		uint32_t _n_outputs;

		uint32_t _systemic_input_latency;
		uint32_t _systemic_output_latency;

		uint64_t _processed_samples;

		pthread_t _main_thread;

		/* process threads */
		static void* dummy_process_thread (void *);
		std::vector<pthread_t> _threads;

		struct ThreadData {
			DummyAudioBackend* engine;
			boost::function<void ()> f;
			size_t stacksize;

			ThreadData (DummyAudioBackend* e, boost::function<void ()> fp, size_t stacksz)
				: engine (e) , f (fp) , stacksize (stacksz) {}
		};

		/* port engine */
		PortHandle add_port (const std::string& shortname, ARDOUR::DataType, ARDOUR::PortFlags);
		int register_system_ports ();
		void unregister_system_ports ();

		std::vector<DummyPort *> _ports;

		bool valid_port (PortHandle port) const {
			return std::find (_ports.begin (), _ports.end (), (DummyPort*)port) != _ports.end ();
		}
		DummyPort * find_port (const std::string& port_name) const {
			for (std::vector<DummyPort*>::const_iterator it = _ports.begin (); it != _ports.end (); ++it) {
				if ((*it)->name () == port_name) {
					return *it;
				}
			}
			return NULL;
		}

}; // class DummyAudioBackend

} // namespace

#endif /* __libbackend_dummy_audiobackend_h__ */