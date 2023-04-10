export type HelloRequest = {
  deviceId: string
}

export type HelloResponse = {
  challenge: string
}

export type AuthRequest = {
  deviceId: string
  response: string
}

export type AuthResponse = {
  success: boolean
}
