interface Props {
  title: string;
  message: string;
}

export default function VizPlaceholder({ title, message }: Props) {
  return (
    <div className="viz-placeholder">
      <strong>{title}</strong>
      <p className="muted">{message}</p>
    </div>
  );
}
