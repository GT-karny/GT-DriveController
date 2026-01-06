try:
    import osi3.osi_sensorview_pb2 as osi_sv
except ImportError:
    # Fallback or error logging if osi3 not available in environment
    print("[GT-DriveController] Error: Could not import osi3.osi_sensorview_pb2")
    osi_sv = None

class Controller:
    def __init__(self):
        print("[GT-DriveController] Initialized Python Controller")

    def update_control(self, binary_data):
        """
        Receives binary SensorView data, returns [throttle, brake, steering]
        """
        if osi_sv is None:
            return [0.0, 0.0, 0.0]

        try:
            # Deserialize OSI
            sv = osi_sv.SensorView()
            sv.ParseFromString(binary_data)
            
            # TODO: Implement actual logic based on sv content
            # For now, just a dummy behavior to prove it works
            
            throttle = 0.5
            brake = 0.0
            steering = 0.01

            # Determine steering based on requested range [-1.0(Right), 1.0(Left)]
            
            return [throttle, brake, steering]

        except Exception as e:
            print(f"[GT-DriveController] Error in update_control: {e}")
            return [0.0, 0.0, 0.0]
