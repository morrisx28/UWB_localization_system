from pycdr2 import IdlStruct
from dataclasses import dataclass
from pycdr2.types import int8, int32, uint32, uint8, float64, float32, sequence, array
from typing import List

@dataclass
class Time(IdlStruct, typename="Time"):
    sec: uint32
    nsec: uint32

@dataclass
class Header(IdlStruct, typename="Header"):
    stamp: Time
    frame_id: str

@dataclass
class JointStates(IdlStruct, typename="JointStates"):
    stamp_sec: uint32
    stamp_nsec: uint32
    frame_id: str
    name: List[str]
    position: List[float64]
    velocity: List[float64]
    effort: List[float64]

@dataclass
class BatteryState(IdlStruct, typename="BatteryState"):
    stamp_sec: uint32
    stamp_nsec: uint32
    frame_id: str
    voltage: float32
    temperature: float32
    current: float32
    charge: float32
    capacity: float32
    design_capacity: float32
    percentage: float32
    power_supply_status: uint8
    power_supply_health: uint8
    power_supply_technology: uint8
    present: bool
    # cell_voltage: List[float32]
    # cell_temperature: List[float32]
    # location: str
    # serial_number: str

@dataclass
class OverViewState(IdlStruct, typename="OverViewState"):
    battery_voltage: float32
    battery_capacity: float32
    battery_percentage: float32
    joint_name: List[str]
    joint_velocity: List[float64]