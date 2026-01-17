// OBS Data Types
export type ObsDataValue =
  | string
  | number
  | boolean
  | ObsData
  | ObsData[]
  | null;

export interface ObsData {
  [key: string]: ObsDataValue;
}

// OBS Property Types
export type ObsPropertyType =
  | 'invalid'
  | 'bool'
  | 'int'
  | 'float'
  | 'text'
  | 'path'
  | 'list'
  | 'color'
  | 'button'
  | 'font'
  | 'editable_list'
  | 'frame_rate'
  | 'group'
  | 'color_alpha'
  | 'unknown';

export type ObsNumberType = 'scroller' | 'slider';

export type ObsTextType = 'default' | 'password' | 'multiline' | 'info';

export type ObsPathType = 'file' | 'file_save' | 'directory';

export type ObsComboType = 'invalid' | 'editable' | 'list' | 'radio';

export type ObsComboFormat = 'invalid' | 'int' | 'float' | 'string';

export interface ObsListItem {
  name: string;
  value: string | number;
  disabled: boolean;
}

export interface ObsPropertyBase {
  name: string;
  description: string;
  type: ObsPropertyType;
  enabled: boolean;
  visible: boolean;
}

export interface ObsIntProperty extends ObsPropertyBase {
  type: 'int';
  min: number;
  max: number;
  step: number;
  number_type: ObsNumberType;
}

export interface ObsFloatProperty extends ObsPropertyBase {
  type: 'float';
  min: number;
  max: number;
  step: number;
  number_type: ObsNumberType;
}

export interface ObsTextProperty extends ObsPropertyBase {
  type: 'text';
  text_type: ObsTextType;
}

export interface ObsPathProperty extends ObsPropertyBase {
  type: 'path';
  path_type: ObsPathType;
  filter: string;
  default_path: string;
}

export interface ObsListProperty extends ObsPropertyBase {
  type: 'list';
  combo_type: ObsComboType;
  combo_format: ObsComboFormat;
  items: ObsListItem[];
}

export interface ObsBoolProperty extends ObsPropertyBase {
  type: 'bool';
}

export interface ObsColorProperty extends ObsPropertyBase {
  type: 'color' | 'color_alpha';
}

export interface ObsButtonProperty extends ObsPropertyBase {
  type: 'button';
}

export interface ObsFontProperty extends ObsPropertyBase {
  type: 'font';
}

export interface ObsEditableListProperty extends ObsPropertyBase {
  type: 'editable_list';
}

export interface ObsFrameRateProperty extends ObsPropertyBase {
  type: 'frame_rate';
}

export interface ObsGroupProperty extends ObsPropertyBase {
  type: 'group';
}

export interface ObsGenericProperty extends ObsPropertyBase {
  type: 'invalid' | 'unknown';
}

export type ObsProperty =
  | ObsIntProperty
  | ObsFloatProperty
  | ObsTextProperty
  | ObsPathProperty
  | ObsListProperty
  | ObsBoolProperty
  | ObsColorProperty
  | ObsButtonProperty
  | ObsFontProperty
  | ObsEditableListProperty
  | ObsFrameRateProperty
  | ObsGroupProperty
  | ObsGenericProperty;

export type Signal = {
  type: string; // Either "output" or "volmeter" or "source".
  id: string; // Signal identifier, e.g. "stop"
  code: number; // 0 for success, other values for errors
  value?: number; // Currently only used for volmeters.
};

export type SceneItemPosition = {
  x: number; // X position in pixels
  y: number; // Y position in pixels
  scaleX: number; // X scaling factor
  scaleY: number; // Y scaling factor
  cropLeft: number; // Pixels to crop from the left
  cropRight: number; // Pixels to crop from the right
  cropTop: number; // Pixels to crop from the top
  cropBottom: number; // Pixels to crop from the bottom
};

export type SourceDimensions = {
  height: number; // Height in pixels, before scaling
  width: number; // Width in pixels, before scaling
};

export type FileExtension = 'mp4' | 'mkv';

interface Noobs {
  Init(
    distPath: string,
    logPath: string,
    cb: (signal: Signal) => void,
  ): void;

  Shutdown(): void;

  // Recording functions.
  SetBuffering(buffering: boolean): void; // In buffering mode, the recording is stored in memory and can be converted to a file later.
  StartBuffer(): void;
  StartRecording(offset: number): void;
  StopRecording(): void;
  ForceStopRecording(): void;
  GetLastRecording(): string;
  SetRecordingCfg(recordingPath: string, fileExtension: FileExtension): void;
  ResetVideoContext(fps: number, width: number, height: number): void;

  // Encoder functions.
  ListVideoEncoders(): string[]; // Returns a list of available video encoders.
  SetVideoEncoder(id: string, settings: ObsData): void; // Create the video encoder to use.

  // Source management functions.
  CreateSource(name: string, type: string, settings?: ObsData): string; // Returns the name of the source, which may vary in the event of a name conflict.
  DeleteSource(name: string): void;
  GetSourceSettings(name: string): ObsData;
  SetSourceSettings(name: string, settings: ObsData): void;
  GetSourceProperties(name: string): ObsProperty[];

  // Audio source management functions.
  SetMuteAudioInputs(mute: boolean): void; // Mute or unmute all audio inputs.
  SetSourceVolume(name: string, volume: number): void; // Set the volume for a specific audio source (0.0 to 1.0).
  SetVolmeterEnabled(enabled: boolean): void; // Enable or disable the volume meter.
  SetAudioSuppression(enabled: boolean): void; // Enable or disable audio suppression (noise gate).
  SetForceMono(enabled: boolean): void; // Enable or disable the force mono audio setting.

  // Scene management functions.
  AddSourceToScene(sourceName: string): void;
  RemoveSourceFromScene(sourceName: string): void;
  // TODO: BEGIN TEMPORARY CODE TO TEST PIPEWIRE
  ShowSource(name: string): void; // Show a source (activates PipeWire streams).
  // TODO: END TEMPORARY CODE TO TEST PIPEWIRE
  GetSourcePos(name: string): SceneItemPosition & SourceDimensions;
  SetSourcePos(name: string, pos: SceneItemPosition): void;

  // Preview functions.
  InitPreview(hwnd: Buffer): void;
  ConfigurePreview(x: number, y: number, width: number, height: number): void;
  ShowPreview(): void;
  HidePreview(): void;
  DisablePreview(): void;
  GetPreviewInfo(): { canvasWidth: number; canvasHeight: number; previewWidth: number; previewHeight: number };
  SetDrawSourceOutline(enabled: boolean): void;
  GetDrawSourceOutlineEnabled(): boolean;
}

declare const noobs: Noobs;
export default noobs;
