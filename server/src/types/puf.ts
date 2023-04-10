export type DeviceId = string

export type PufTable = Record<DeviceId, string>

export type PufRequests = Record<DeviceId, { challenge: string; response: string }>
